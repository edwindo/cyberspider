#ifndef DISKMULTIMAP_H_
#define DISKMULTIMAP_H_

#include <string>
#include "MultiMapTuple.h"
#include "BinaryFile.h"
#include <queue>

class DiskMultiMap
{
public:
    
    class Iterator
    {
    public:
        Iterator();
        Iterator(const std::string& key, queue<MultiMapTuple>& associationsQueue); //overloaded constructor that takes in key and a queue of offsets
        // You may add additional constructors
        bool isValid() const;
        Iterator& operator++();
        MultiMapTuple operator*();
        
    private:
        bool m_isValid;
        MultiMapTuple m_currentAssociations;
        const char* m_key;
        queue<MultiMapTuple> m_associationsQueue;
    };
    
    DiskMultiMap();
    ~DiskMultiMap();
    bool createNew(const std::string& filename, unsigned int numBuckets);
    bool openExisting(const std::string& filename);
    void close();
    bool insert(const std::string& key, const std::string& value, const std::string& context);
    Iterator search(const std::string& key);
    int erase(const std::string& key, const std::string& value, const std::string& context);
    
private:
    BinaryFile m_bf;
    unsigned int m_numBuckets;
    const unsigned int m_offsetSize = sizeof(BinaryFile::Offset);
    struct MultiMapNode
    {
        MultiMapNode(const char* k, const char* v, const char* c, BinaryFile::Offset n): next(n)
        {
            strcpy(key, k);
            strcpy(value, v);
            strcpy(context, c);
        }
        char key[121];
        char value[121];
        char context[121];
        BinaryFile::Offset next;
    };
    const unsigned int m_nodeSize = sizeof(MultiMapNode); //368
    BinaryFile::Offset m_firstUnused; //first offset that is completely unused
    BinaryFile::Offset m_freedNodes; //list of nodes that have previously been freed and should be reused
    const unsigned int m_hashTableStart = 12; //change to constant?
    
    //helper functions
    void addToUnusedNodes(BinaryFile::Offset offset);
    BinaryFile::Offset generateOpenOffset();
    
};



#endif // DISKMULTIMAP_H_
