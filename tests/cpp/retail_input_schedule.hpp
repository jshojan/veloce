#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
// Original diagnostic format: decimal scheduling quantum, hexadecimal virtual
// port-zero digital mask. A transition remains active until the next transition.
class RetailInputSchedule {
    struct Event {unsigned quantum;uint32_t mask;};
    std::vector<Event> events;
public:
    void load(const char* path){
        std::ifstream file(path);if(!file)throw std::runtime_error("input script unavailable");
        std::string line;while(std::getline(file,line)){
            line=line.substr(0,line.find('#'));std::istringstream fields(line);std::string quantum,mask,extra;
            if(!(fields>>quantum))continue;
            if(!(fields>>mask) || fields>>extra)throw std::runtime_error("invalid input fields");
            unsigned q=0;uint32_t m=0;const auto a=std::from_chars(quantum.data(),quantum.data()+quantum.size(),q,10),b=std::from_chars(mask.data(),mask.data()+mask.size(),m,16);
            if(a.ec!=std::errc{} || a.ptr!=quantum.data()+quantum.size() || b.ec!=std::errc{} || b.ptr!=mask.data()+mask.size() || q>=100000 || (!events.empty() && q<=events.back().quantum))throw std::runtime_error("invalid input transition");
            events.push_back({q,m});
        }
        if(file.bad())throw std::runtime_error("input script read failed");
    }
    uint32_t at(unsigned quantum) const {
        const auto it=std::upper_bound(events.begin(),events.end(),quantum,[](unsigned q,const Event& event){return q<event.quantum;});return it==events.begin()?0:(it-1)->mask;
    }
};
