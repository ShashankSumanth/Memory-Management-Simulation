#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <iostream>
using namespace std;

struct Page{
    bool used{true};
    int vpn{-1};
};

struct PTE{                                                     //Page table entry
    bool valid{false};
    bool swapInfo{true};
    int pfn{-1};
};

enum class RequestStatus{
    successfullyFetched,
    accessViolation,                                            //Segmentation Fault
};


class MainMemory{                                               //i.e. RAM

    private:
    int m_size{};
    int m_pageSize{};
    int m_utilized{0};
    int m_clockPtr{0};
    vector<Page>m_pages;
    

    public:
    MainMemory(int size, int pageSize)
        : m_size{size}
        , m_pageSize{pageSize}
    {
        m_pages.assign(size/pageSize,Page());
    }

    pair<int,int> loadPageIntoMainMemory(int vpn){
        int pfn {m_utilized};
        int swappedOut {-1};
        m_utilized++;
        if (m_utilized == m_pages.size()){
            pfn = free();
            swappedOut = m_pages[pfn].vpn;
            m_utilized--;
        }
        m_pages[pfn] = Page{true,vpn};
        return {pfn,swappedOut};
    }

    int free(){                                     //implements the clock algo to free up the main memory
        while(m_pages[m_clockPtr].used){
            m_pages[m_clockPtr].used=false;
            m_clockPtr++;
            if(m_clockPtr>=m_pages.size())m_clockPtr=0;
        }
        return m_clockPtr;
    }

};

class TLB{                                                      //Translation Lookaside Buffer

    private:
    int m_maxSize{};
    unordered_map<int,int>m_cache{};
    vector<int>m_cacheEntries{};
    
    int getRand(){                                              //used for random eviction
        int randomIdx = rand()%(m_maxSize);
        return randomIdx;
    }

    public:
    TLB(int maxSize)
    : m_maxSize{maxSize}
    {
    }

    bool searchPFN(int vpn){
        if (m_cache.count(vpn)) return true;
        else return false;
    }

    int retrievePFN(int vpn){
        return m_cache[vpn];
    }

    void updateCache(int vpn, int pfn){
        if(m_cacheEntries.size() >= m_maxSize){
            int randIdx = getRand();
            m_cache.erase(m_cacheEntries[randIdx]);
            m_cacheEntries[randIdx] = vpn;
            cout << "evicted from tlb " << randIdx << "\n";
        } else {
            m_cacheEntries.push_back(vpn);
        }
        m_cache[vpn] = pfn;
    }

};

class PageTable{
    
    private:
    int m_size{};
    vector<PTE>m_table;

    public:
    PageTable(int size)
        : m_size{size}
        , m_table(size)
    {
    }

    bool checkValidity(int vpn){
        if(vpn >= m_size || !m_table[vpn].valid)return false;
        else return true;
    }

    bool checkSwapinfo(int vpn){
        if (m_table[vpn].swapInfo)return true;
        else return false;
    }

    void setSwap(int vpn){
        m_table[vpn].swapInfo = true;
    }

    void resetSwap(int vpn){
        m_table[vpn].swapInfo = false;
    }

    void setValid(int vpn){
        m_table[vpn].valid = true;
    }

    void resetValid(int vpn){
        m_table[vpn].valid = false;
    }

    int retrievePFN(int vpn){
        return m_table[vpn].pfn;
    }

    void update(int vpn, int pfn){
        m_table[vpn].pfn = pfn;
    }

};

class SwapSpace{

    private:
    int m_size{};
    unordered_set<int>m_swappedPages{};

    public:
    SwapSpace(int size)
        : m_size{size}
    {
        for(int page = 0; page<size; page++){
            loadIntoSwap(page);
        }
    }
    void loadIntoSwap(int vpn){
        m_swappedPages.insert(vpn);
    }

    void evictFromSwap(int vpn){
        m_swappedPages.erase(vpn);
    }

};

class MemoryManager{

    private:
    MainMemory m_mainMemory;
    TLB m_tlb;
    PageTable m_pageTable;
    SwapSpace m_swapSpace;

    public:
    MemoryManager(int ram, int pageSize, int tlbSize, int addressSpace)
        : m_mainMemory{ram,pageSize}
        , m_tlb{tlbSize}
        , m_pageTable{addressSpace/pageSize}
        , m_swapSpace{addressSpace/pageSize}
    {
    }

    pair<RequestStatus,int> requestPFN(int vpn){
        while (true){
            if (m_tlb.searchPFN(vpn)){                                                      //TLB hit
                cout << "Found in TLB " << vpn << " \n";
                return {RequestStatus::successfullyFetched,m_tlb.retrievePFN(vpn)};
            } else {                 
                cout << "Not found in TLB\n";                                                       //TLB miss
                if (m_pageTable.checkValidity(vpn)){                                        //Page table hit
                    cout << "Page table hit\n";
                    int pfn {m_pageTable.retrievePFN(vpn)};
                    m_tlb.updateCache(vpn,pfn);
                    continue;
                } else {                                                                    //Page fault
                    cout << "Page fault\n";
                    if (m_pageTable.checkSwapinfo(vpn)){
                        cout << "Found in swap space\n";
                        pair<int,int>loadResult {m_mainMemory.loadPageIntoMainMemory(vpn)};
                        int pfn {loadResult.first};
                        int swappedOut {loadResult.second};
                        m_swapSpace.evictFromSwap(vpn);
                        m_pageTable.resetSwap(vpn);
                        m_pageTable.setValid(vpn);
                        m_pageTable.update(vpn,pfn);
                        if(swappedOut != -1){                                               //Page evicted from main memory
                            cout << "Page evcited\n";
                            m_swapSpace.loadIntoSwap(swappedOut);
                            m_pageTable.setSwap(swappedOut);
                            m_pageTable.resetValid(swappedOut);
                        }
                        continue;
                    } else {
                        cout << "Access vio\n";
                        return {RequestStatus::accessViolation, -1};
                    }
                }
            }
        }
    }

};


int main(){
    MemoryManager memoryManager{16,2,8,64};
    vector<int> pageRequests{1,2,3,4,5,6,7,8,9,10,11,12,1,16};
    for(int page:pageRequests){
        memoryManager.requestPFN(page);
    }
}