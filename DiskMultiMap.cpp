#include "DiskMultiMap.h"
#include "BinaryFile.h"
#include "MultiMapTuple.h"
#include <functional>
#include <string>
#include <queue>
#include <iostream>


DiskMultiMap::DiskMultiMap()
{
    
}

DiskMultiMap::~DiskMultiMap()
{
    if (m_bf.isOpen())
    {
        m_bf.close();
    }
}

bool DiskMultiMap::createNew(const std::string& filename, unsigned int numBuckets)
{
    if (m_bf.isOpen()) //if the current binary file is open, close it
        m_bf.close();
    if (!m_bf.createNew(filename)) //if unable to create a new file, return false
        return false;
    m_numBuckets = numBuckets;
    m_firstUnused = m_numBuckets * m_offsetSize + m_hashTableStart;
    m_freedNodes = -1;
    for (int i = m_hashTableStart; i < numBuckets*m_offsetSize+m_hashTableStart; i+= m_offsetSize) //load a offset variable for the number of buckets, representing an "array" of each value in the table
    {
        BinaryFile::Offset temp = -1;
        m_bf.write(temp, i);
    }
    return true; //if able to create new binary file and create "array" of buckets, return true to indicate success
}

bool DiskMultiMap::openExisting(const std::string& filename)
{
    if (m_bf.isOpen()) //if there is currently a binary file open, close it
    {
        m_bf.close();
    }
    if(!m_bf.openExisting(filename)) // attempt to open exisiting file with parameter name, return if it is successful
    {
        return false;
    }
    m_bf.read(m_firstUnused, 0);
    m_bf.read(m_freedNodes, sizeof(BinaryFile::Offset));
    m_bf.read(m_numBuckets, 2*sizeof(BinaryFile::Offset));
    return true;
}

void DiskMultiMap::close()
{
    m_bf.write(m_firstUnused, 0);
    m_bf.write(m_freedNodes, sizeof(BinaryFile::Offset));
    m_bf.write(m_numBuckets, 2*sizeof(BinaryFile::Offset));
    if (m_bf.isOpen())
    {
        m_bf.close();
    }
}

bool DiskMultiMap::insert(const std::string& key, const std::string& value, const std::string& context)
{
    if (key.size() > 120 || value.size() > 120 || context.size() > 120)
    {
        return false;
    }
    
    hash<string> stringHash; //generate hash value for key string
    int hashValue = stringHash(key)%m_numBuckets;
    BinaryFile::Offset bucket;
    m_bf.read(bucket, hashValue * m_offsetSize + m_hashTableStart);
    if (bucket == -1) //case if the bucket is currently empty
    {
        if (m_freedNodes != -1) //if the freed nodes list is not empty
        {
            BinaryFile::Offset freeOffset = generateOpenOffset();
            m_bf.write(freeOffset,hashValue * m_offsetSize + m_hashTableStart); //set the bucket to one of the freed node
            
            m_bf.write(MultiMapNode(key.c_str(), value.c_str(), context.c_str(), -1), freeOffset); //create the node in that freed space
        }
        else //otherwise set the bucket to the next open space on the disk and create a new node at that space
        {
            m_bf.write(m_firstUnused, hashValue * m_offsetSize + m_hashTableStart); //sets the value in the hash table to the new node
            m_bf.write(MultiMapNode(key.c_str(), value.c_str(), context.c_str(), -1), m_firstUnused); //writes a new node at the furthest open spot
            m_firstUnused += m_nodeSize; //shift the first unused offset by the size of the node
        }
    }
    else //if the bucket is currently being used, access the node is points to and add the node to the end of that
    {
        MultiMapNode temp("","","",0); //temporary node with dummy values used to traverse
        m_bf.read(temp, bucket); //temp is now set to the first node in the list that the bucket leads to
        BinaryFile::Offset offsetOfTemp = bucket; //stores the offset of temp so it can be changed
        while (temp.next != -1) //while the node continues to point to more nodes
        {
            offsetOfTemp = temp.next;
            m_bf.read(temp, temp.next); //set temp to the next node in the list
        }
        //temp is now the last node in the list, consider both cases if there are freed nodes or not
        if (m_freedNodes != -1)
        {
            BinaryFile::Offset freeOffset = generateOpenOffset();
            m_bf.write(MultiMapNode(temp.key, temp.value, temp.context, freeOffset),offsetOfTemp); //update the last node's next offset to be the new node
            m_bf.write(MultiMapNode(key.c_str(), value.c_str(), context.c_str(), -1), freeOffset); //create the node in that freed space
        }
        else //if there are no free nodes, create the new node by expanding the disk
        {
            m_bf.write(MultiMapNode(temp.key, temp.value, temp.context, m_firstUnused),offsetOfTemp); //update the last node's next offset to be the new node
            m_bf.write(MultiMapNode(key.c_str(), value.c_str(), context.c_str(), -1), m_firstUnused); //writes a new node at the furthest open spot
            m_firstUnused += m_nodeSize; //shift the first unused offset by the size of the node
        }
        
        
    }
    return true;
}

DiskMultiMap::Iterator DiskMultiMap::search(const std::string& key)
{
    hash<string> stringHash; //generate hash value for key string and set bucket to the offset that the key string leads to
    int hashValue = stringHash(key)%m_numBuckets;
    BinaryFile::Offset bucket;
    m_bf.read(bucket, hashValue * m_offsetSize + m_hashTableStart);
    
    if (bucket == -1) //if the key string leads to an empty bucket, return an invalid iterator
        return Iterator(); //default iterator constructor that start invalid
    
    queue<MultiMapTuple> queueOfAssociations;
    
    MultiMapNode temp("","","",0); //temporary node with dummy values used to traverse and access values from disk
    
    m_bf.read(temp, bucket); //temp is set to the node that bucket points to
    while (temp.next != -1) //while temp's next offset is not terminating (stops before checking LAST node in list)
    {
        if (strcmp(temp.key,key.c_str()) == 0) //if the key of this node matches the search key
        {
            MultiMapTuple addToQueue = {temp.key, temp.value, temp.context};
            queueOfAssociations.push(addToQueue);
        }
        m_bf.read(temp, temp.next); //set temp to the next node
    }
    if (strcmp(temp.key,key.c_str()) == 0) //since the previous node does not check the last node in the list, one last check
    {
        MultiMapTuple addToQueue = {temp.key, temp.value, temp.context};
        queueOfAssociations.push(addToQueue);
    }
    
    if (queueOfAssociations.empty()) //if this queue is empty, there were no associations found of the given key, therefore an invalid iterator will be returned
        return Iterator();
    
    return Iterator(key, queueOfAssociations); //if the queue is not empty call the overloaded constructor for the iterator and return that. (Iterator will be valid)
    
    
}

int DiskMultiMap::erase(const std::string& key, const std::string& value, const std::string& context)
{
    int numRemovals = 0;
    hash<string> stringHash; //generate hash value for key string
    int hashValue = stringHash(key)%m_numBuckets;
    BinaryFile::Offset bucket;
    m_bf.read(bucket, hashValue*m_offsetSize + m_hashTableStart); //sets bucket variable to the corresponding bucket from the hash value
    
    if (bucket == -1) //if the key string does not map to and value, return numRemovals(which is 0) since there is nothing to remove
        return numRemovals;
    
    MultiMapNode temp("","","",0); //temporary node with dummy values to be replaced
    m_bf.read(temp, bucket); //set temp to the node that bucket points to
    
    BinaryFile::Offset currentNode = bucket;
    //the following loop covers the case of the first node the hash table points to being removed, since the offset value of the hashtable will need to be changed
    
    while (strcmp(temp.key, key.c_str()) == 0 && strcmp(temp.value, value.c_str()) == 0 && strcmp(temp.context, context.c_str()) == 0) //while the temp node matches the parameter values
    {
        addToUnusedNodes(currentNode); //add the current node to the unused nodes
        m_bf.write(temp.next, hashValue*m_offsetSize + m_hashTableStart); // set the bucket in the hash table to point to the next node since the previous one was removed
        numRemovals++;  //increment the number of removed associations
        if (temp.next == -1) //if there is no next node, return the number of removals
            return numRemovals;
        currentNode = temp.next;
        m_bf.read(temp, temp.next); //set temp to the next node
    }
    
    //the next loop no longer needs to make changes to the hash table but only the adjacent node
    
    MultiMapNode nextNode("","","",0); //temp node representing the next node with dummy values
    m_bf.read(nextNode, temp.next);
    while (temp.next != -1) //while the next node exists
    {
        m_bf.read(nextNode, temp.next);
        if (strcmp(nextNode.key, key.c_str()) == 0 && strcmp(nextNode.value, value.c_str()) == 0 && strcmp(nextNode.context, context.c_str()) == 0) //if the next node values match the parameter
        {
            addToUnusedNodes(temp.next); //add the removed node to the unused nodes
            m_bf.write(MultiMapNode(temp.key, temp.value, temp.context, nextNode.next),currentNode); //set the current node to point to the next node's next
            numRemovals++; //increment the number of removed associations
            currentNode = temp.next; //set sthe current node to the next node
            m_bf.read(temp, temp.next); //set temp to the next node
        }
        else
        {
            currentNode = temp.next;
            m_bf.read(temp,temp.next);
        }
        
    } //this loop concludes when temp reaches the last node (which has already been checked since the NEXT node is what is checked)
    
    return numRemovals; //the number of removed associations should be counted by this variable
    
    
}
//private DiskMultiMap helper functions

BinaryFile::Offset DiskMultiMap::generateOpenOffset()
{
    if (m_freedNodes == -1) //if the list is empty, return -1
        return -1;
    MultiMapNode temp("","","",0); //dummy values to be replaced
    m_bf.read(temp, m_freedNodes);
    BinaryFile::Offset freeOffset = m_freedNodes;
    m_freedNodes = temp.next; //sets the m_unusedNode to the next in the list
    return freeOffset;
}

void DiskMultiMap::addToUnusedNodes(BinaryFile::Offset offset)
{
    if (m_freedNodes == -1) //case for if the list is initally empty
    {
        m_freedNodes = offset;
        m_bf.write(MultiMapNode("","","", -1),offset); //generates a node with dummy values and a terminating next offset
        return;
    }
    m_bf.write(MultiMapNode("","","", m_freedNodes), offset); //creates a node with dummy value, offset is the old head pointer set at the offset parameter
    m_freedNodes = offset; //sets the new head as the offset
}

//Iterator class implementation

DiskMultiMap::Iterator::Iterator()
{
    m_isValid = false;
}

DiskMultiMap::Iterator::Iterator(const std::string& key, queue<MultiMapTuple>& associationsQueue)
{
    m_isValid = true;
    m_key = key.c_str();
    
    //set first associationQueue value to current association
    m_currentAssociations.key = associationsQueue.front().key;
    m_currentAssociations.value = associationsQueue.front().value;
    m_currentAssociations.context = associationsQueue.front().context;
    m_associationsQueue = associationsQueue; //set the member queue to equal the parameter queue
    m_associationsQueue.pop(); //pop off the front value
}

bool DiskMultiMap::Iterator::isValid() const
{
    return m_isValid;
}

DiskMultiMap::Iterator& DiskMultiMap::Iterator::operator++()
{
    if (!m_isValid) //if not valid, do nothing
        return (*this);
    
    if (m_associationsQueue.empty()) //if there are no more associations with a matching key, set invalid and return
    {
        m_isValid = false;
        return (*this);
    }
    
    m_currentAssociations.key = m_associationsQueue.front().key;
    m_currentAssociations.value = m_associationsQueue.front().value;
    m_currentAssociations.context = m_associationsQueue.front().context;
    m_associationsQueue.pop(); //pop off the front value

    
    return (*this); //return reference to iterator
    
}

MultiMapTuple DiskMultiMap::Iterator::operator*()
{
    if (!isValid())
    {
        MultiMapTuple empty = {"","",""};
        return empty;
    }
    return m_currentAssociations;
}


/*int main() {
    DiskMultiMap x;
    x.createNew("myhashtable.dat",100); // empty, with 100 buckets
    x.insert("a.exe", "b.exe", "comp1");
    x.insert("b.exe", "c.exe", "comp2");
    x.insert("a.exe", "g.exe", "comp3");
    
    DiskMultiMap::Iterator it = x.search("a.exe");
    if (it.isValid())
    {
        cout << "I found at least 1 item with a key of hmm.exe\n";
        do
        {
            MultiMapTuple m = *it; // get the association
            cout << "The key is: " << m.key << endl;
            cout << "The value is: " << m.value << endl;
            cout << "The context is: " << m.context << endl;
            cout << endl;
            ++it; // advance iterator to the next matching item
        } while (it.isValid());
    }
    x.close();

}*/