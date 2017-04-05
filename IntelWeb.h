#ifndef INTELWEB_H_
#define INTELWEB_H_

#include "InteractionTuple.h"
#include "DiskMultiMap.h"
#include <string>
#include <vector>

class IntelWeb
{
public:
    IntelWeb();
    ~IntelWeb();
    bool createNew(const std::string& filePrefix, unsigned int maxDataItems);
    bool openExisting(const std::string& filePrefix);
    void close();
    bool ingest(const std::string& telemetryFile);
    unsigned int crawl(const std::vector<std::string>& indicators,
                       unsigned int minPrevalenceToBeGood,
                       std::vector<std::string>& badEntitiesFound,
                       std::vector<InteractionTuple>& interactions
                       );
    bool purge(const std::string& entity);
    
private:
    DiskMultiMap m_sourceToDestination, m_destinationToSource;
    //helper function
    bool isPrevalent(std::string, unsigned int threshold);
    
    // Your private member declarations will go here
};


inline
bool operator<(const InteractionTuple& a, const InteractionTuple& b)
{
    if (a.context != b.context)
        return a.context < b.context;
    if (a.from != b.from)
        return a.from < b.from;
    if (a.to != b.to)
        return a.to < b.to;
    
    return false;
}

#endif // INTELWEB_H_
