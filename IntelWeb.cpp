#include "IntelWeb.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <set>
#include <queue>
using namespace std;

IntelWeb::IntelWeb()
{
    
}

IntelWeb::~IntelWeb()
{
    close();
}

bool IntelWeb::createNew(const std::string& filePrefix, unsigned int maxDataItems)
{
    //create new DiskMultiMaps with given prefix and sizes, if any fail to create, close any other open DiskMultiMaps and return false.
    if (!m_sourceToDestination.createNew(filePrefix+".sourceToDestination", maxDataItems*2))
    {
        return false;
    }
    if(!m_destinationToSource.createNew(filePrefix+".destinationToSource", maxDataItems*2))
    {
        m_sourceToDestination.close();
        return false;
    }
    
    return true;
    
}

bool IntelWeb::openExisting(const std::string& filePrefix)
{
    if(!m_sourceToDestination.openExisting(filePrefix+".sourceToDestination"))
    {
        return false;
    }
    if (!m_destinationToSource.openExisting(filePrefix+".destinationToSource"))
    {
        m_sourceToDestination.close();
        return false;
    }
    return true;
}

void IntelWeb::close()
{
    m_sourceToDestination.close();
    m_destinationToSource.close();
}

bool IntelWeb::ingest(const std::string& telemetryFile)
{
    ifstream inf(telemetryFile); //open file for input
    
    if (!inf)
    {
        cout << "Cannot open file" << endl;
        return false;
    }
    
    string line;
    while (getline(inf, line))
    {
        istringstream iss(line);
        string key, value, context;
        if (! (iss >> context >> key >> value))
        {
            cout << "Badly formatted line: " << line << endl;
            continue;
        }
        
        /*char dummy;
        if (iss >> dummy) // succeeds if there a non-whitespace char
            cout << "Ignoring extra data in line: " << line << endl;*/
        
        //Each pair of interations is stored into the DiskMultiMaps in both orders
        
        m_sourceToDestination.insert(key, value, context);
        m_destinationToSource.insert(value, key, context);
        
    }
    
    
    
    
    return true;
    
}

unsigned int IntelWeb::crawl(const std::vector<std::string>& indicators, unsigned int minPrevalenceToBeGood, std::vector<std::string>& badEntitiesFound, std::vector<InteractionTuple>& interactions)
{
    set<std::string> badEntitiesSet; //set representing all the currently known bad entities
    set<InteractionTuple> interactionsSet; //set representing all the associations;
    queue<std::string> maliciousAssociationsQueue; //a queue used simulate a breadth first search through associations
    set<std::string> knownGoodEntites; //set of entites that are known to be above the threshold
    
    //start by enqueuing each of the indicator entities from the vector into the queue
    for (const std::string s: indicators)
    {
        maliciousAssociationsQueue.push(s);
    }
    
    while (!maliciousAssociationsQueue.empty()) //while the queue is not empty
    {
        bool foundOneAssociation = false; //bool represeting if there were any associations found with this entity only relevant for the initial indicators list
        std::string maliciousEntity = maliciousAssociationsQueue.front(); //set temporary variable to front of queue
        maliciousAssociationsQueue.pop(); //pop value off queue
        
        if (knownGoodEntites.count(maliciousEntity) == 1) //if the entity is present in the set of knownGoodEntities
        {
            badEntitiesSet.erase(maliciousEntity);
            continue; //do not check for associations with this entity
        }
        if (isPrevalent(maliciousEntity, minPrevalenceToBeGood)) //check the number of occurances of the entity, if it is high enough, add it to the set of known entites and do not check for associations with this entity
        {
            badEntitiesSet.erase(maliciousEntity);
            knownGoodEntites.insert(maliciousEntity);
            continue;
        }
        
        DiskMultiMap::Iterator sources = m_sourceToDestination.search(maliciousEntity); //iterator for the diskMultiMap of source to destination
        DiskMultiMap::Iterator destinations = m_destinationToSource.search(maliciousEntity); //iterator for the diskMap of destination to source
        
        while (sources.isValid()) //while sources points to a node with a matching key
        {
            foundOneAssociation = true;
            MultiMapTuple tempMultiMapT = *sources; //temporary MultiMapTuple that stores the key, value, and context of the node with matching key
            if (badEntitiesSet.count(tempMultiMapT.value) != 1) //if the value being checked is not already a known bad entity
            {
                badEntitiesSet.insert(tempMultiMapT.value); //add this new value as a bad entity
                maliciousAssociationsQueue.push(tempMultiMapT.value); //add this malicious value to the queue to search through its associations
            }
            interactionsSet.insert(InteractionTuple(tempMultiMapT.key, tempMultiMapT.value, tempMultiMapT.context)); //store the interaction
            ++sources; //increment to the next node (if there is one)
        }
        
        //the following while loop does the exact same process as the above loop with the other multimap
        while (destinations.isValid())
        {
            foundOneAssociation = true;
            MultiMapTuple tempMultiMapT = *destinations;
            if (badEntitiesSet.count(tempMultiMapT.value) != 1)
            {
                badEntitiesSet.insert(tempMultiMapT.value);
                maliciousAssociationsQueue.push(tempMultiMapT.value);
            }
            interactionsSet.insert(InteractionTuple(tempMultiMapT.value, tempMultiMapT.key, tempMultiMapT.context)); //value and key are swapped since the format of the interaction tuple is from,to,context
            ++destinations;
        }
        
        if (foundOneAssociation) //this is only relevant for the initial list of indicators, since if there are no associations with them, they are not added to the list of bad entities found
            badEntitiesSet.insert(maliciousEntity);
        
    }
    //when the loop ends all bad entities should be in badEntitiesSet and all interations involving them should be in interactionsSet
    
    badEntitiesFound.clear(); //any extraneous pre-existing values in the vector are cleared
    interactions.clear();
    
    set<std::string>::iterator badEntitiesIterator = badEntitiesSet.begin(); //load all values in the bad entities set into the vector (will be in order)
    while (badEntitiesIterator != badEntitiesSet.end())
    {
        badEntitiesFound.push_back(*badEntitiesIterator);
        badEntitiesIterator++;
    }
    
    set<InteractionTuple>::iterator interactionsIterator = interactionsSet.begin(); //load all values in the interactions set into the vector (will be in order)
    while (interactionsIterator != interactionsSet.end())
    {
        interactions.push_back(*interactionsIterator);
        interactionsIterator++;
    }
    
    return static_cast<int>(badEntitiesFound.size()); //casted to remove warning
    
}

bool IntelWeb::purge(const std::string& entity)
{
    bool atLeastOneRemoved = false;
    DiskMultiMap::Iterator sources = m_sourceToDestination.search(entity); //iterators through both maps for the parameter key
    
    
    while (sources.isValid()) //loop through all the key matches in disk multi map and erase them
    {
        atLeastOneRemoved = true;
        MultiMapTuple tempMMT = *sources;
        m_sourceToDestination.erase(tempMMT.key, tempMMT.value, tempMMT.context); //erase from the current multi map
        m_destinationToSource.erase(tempMMT.value, tempMMT.key, tempMMT.context); //erase from the opposite multimap by flipping order of key and value
        ++sources;
    }
    
    DiskMultiMap::Iterator destinations = m_destinationToSource.search(entity);
    while (destinations.isValid())
    {
        atLeastOneRemoved = true;
        MultiMapTuple tempMMT = *destinations;
        m_destinationToSource.erase(tempMMT.key, tempMMT.value, tempMMT.context);
        m_sourceToDestination.erase(tempMMT.value, tempMMT.key, tempMMT.context); //erase from opposite multimap by flipping order of key and value
        ++destinations;
    }
    
    return atLeastOneRemoved;
    
}

/*
int main()
{
    
    IntelWeb iW;
    iW.createNew("file", 100);
    std::string filename = "/Users/Edwin/Desktop/UCLA\ Computer\ Science/CS32\ Projects/Project\ 4/telemetry.txt";
    iW.ingest(filename);
    vector<std::string> indicators;
    indicators.push_back("a.exe");
    indicators.push_back("www.rare-malicious-website.com");
    vector<std::string> badEntitiesFound;
    vector<InteractionTuple> interactions;
    cout << iW.crawl(indicators, 10, badEntitiesFound, interactions) << endl;
    cout << endl;
    
    
    for (std::string s: badEntitiesFound)
        cout << s << endl;
    cout << endl;
    for (InteractionTuple i: interactions)
    {
        cout << i.context << " " << i.from << " " << i.to << endl;
    }
    cout << endl;
    iW.purge("www.attacker.com");
    cout << iW.crawl(indicators, 10, badEntitiesFound, interactions) << endl;
    for (std::string s: badEntitiesFound)
        cout << s << endl;
    cout << endl;
    for (InteractionTuple i: interactions)
    {
        cout << i.context << " " << i.from << " " << i.to << endl;
    }

}*/

//helper functions

bool IntelWeb::isPrevalent(string entity, unsigned int threshold)
{
    unsigned int numOccurances = 0;
    DiskMultiMap::Iterator sources = m_sourceToDestination.search(entity);
    DiskMultiMap::Iterator destinations = m_destinationToSource.search(entity);
    while (sources.isValid())
    {
        numOccurances++;
        ++sources;
    }
    while (destinations.isValid())
    {
        numOccurances++;
        ++destinations;
    }
    return numOccurances >= threshold;
}

