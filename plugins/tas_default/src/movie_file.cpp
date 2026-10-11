#include "movie_file.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

namespace emu::tas {
namespace {
constexpr size_t max_frames=1000000,max_state=64*1024*1024,max_file=100*1024*1024;
struct Reader {
    const std::vector<uint8_t>& bytes;size_t at=0;bool good=true;
    uint64_t number(size_t count) {
        if(count>bytes.size()-at){good=false;return 0;}
        uint64_t n=0;for(size_t i=0;i<count;++i)n|=uint64_t(bytes[at++])<<(8*i);return n;
    }
    void raw(void* output,size_t count) {
        if(count>bytes.size()-at){good=false;return;}
        std::memcpy(output,bytes.data()+at,count);at+=count;
    }
};
template<size_t N> bool terminated(const char (&s)[N]) {return std::memchr(s,0,N)!=nullptr;}
bool valid(const Movie& m) {
    if(m.frames.size()>max_frames || m.start_state.size()>max_state ||
       m.info.starts_from_savestate!=!m.start_state.empty())return false;
    if(!terminated(m.info.title)||!terminated(m.info.author)||!terminated(m.info.description)||
       !terminated(m.info.platform)||!terminated(m.info.rom_name))return false;
    for(size_t i=0;i<m.frames.size();++i){const auto& f=m.frames[i];
        if(f.frame_number!=i || (f.has_savestate && (f.savestate_slot<0 || f.savestate_slot>9)))return false;
    }return true;
}
void strings(Reader& r,TASMovieInfo& i) {
    r.raw(i.title,sizeof(i.title));r.raw(i.author,sizeof(i.author));r.raw(i.description,sizeof(i.description));
    r.raw(i.platform,sizeof(i.platform));r.raw(i.rom_name,sizeof(i.rom_name));
}
bool tas2(const std::vector<uint8_t>& bytes,Movie& m) {
    Reader r{bytes,4};strings(r,m.info);m.info.rom_crc32=r.number(4);m.info.frame_count=r.number(8);m.info.rerecord_count=r.number(8);
    auto state_flag=r.number(1),state_size=r.number(4);
    if(!r.good || state_flag>1 || state_size>max_state || m.info.frame_count>max_frames)return false;
    m.info.starts_from_savestate=state_flag;
    if(state_size>bytes.size()-r.at || m.info.frame_count>(bytes.size()-r.at-state_size)/29)return false;
    if(bytes.size()-r.at!=state_size+29*m.info.frame_count)return false;
    m.start_state.resize(state_size);if(state_size)r.raw(m.start_state.data(),state_size);
    m.frames.resize(m.info.frame_count);
    for(auto& f:m.frames){f.frame_number=r.number(8);for(auto& input:f.controller_inputs)input=r.number(4);
        auto flags=r.number(1);if(flags>3)return false;f.has_reset=flags&1;f.has_savestate=flags&2;
        auto slot=r.number(4);f.savestate_slot=slot<=0x7fffffff?int32_t(slot):int32_t(int64_t(slot)-0x100000000LL);
    }return r.good && valid(m);
}
bool tas1(const std::vector<uint8_t>& bytes,Movie& m) {
    Reader r{bytes,4};std::array<uint8_t,sizeof(TASMovieInfo)> header{};r.raw(header.data(),header.size());
    if(!r.good || header[offsetof(TASMovieInfo,starts_from_savestate)]>1)return false;
    std::memcpy(&m.info,header.data(),header.size());
    if(m.info.frame_count>max_frames)return false;
    uint32_t size=0;if(m.info.starts_from_savestate)r.raw(&size,sizeof(size));
    if(!r.good || size>max_state || size>bytes.size()-r.at ||
       m.info.frame_count>(bytes.size()-r.at-size)/sizeof(TASFrameData) ||
       bytes.size()-r.at!=size+m.info.frame_count*sizeof(TASFrameData))return false;
    m.start_state.resize(size);if(size)r.raw(m.start_state.data(),size);
    m.frames.resize(m.info.frame_count);
    for(auto& f:m.frames){std::array<uint8_t,sizeof(TASFrameData)> raw{};r.raw(raw.data(),raw.size());
        if(raw[offsetof(TASFrameData,has_reset)]>1 || raw[offsetof(TASFrameData,has_savestate)]>1)return false;
        std::memcpy(&f,raw.data(),raw.size());
    }return r.good && valid(m);
}
bool integer(const std::string& s,int32_t& n){auto r=std::from_chars(s.data(),s.data()+s.size(),n);return r.ec==std::errc{} && r.ptr==s.data()+s.size();}
template<size_t N> void text(char (&dest)[N],const std::string& source){std::memcpy(dest,source.data(),std::min(N-1,source.size()));dest[std::min(N-1,source.size())]=0;}
uint32_t buttons(uint8_t nes) {
    constexpr VirtualButton map[]={VirtualButton::A,VirtualButton::B,VirtualButton::Select,VirtualButton::Start,VirtualButton::Up,VirtualButton::Down,VirtualButton::Left,VirtualButton::Right};
    uint32_t mask=0;for(unsigned i=0;i<8;++i)if(nes&(1u<<i))mask|=button_to_mask(map[i]);return mask;
}
bool fm2(const std::vector<uint8_t>& bytes,Movie& m) {
    std::map<std::string,std::string> headers;size_t at=0;
    while(at<bytes.size() && bytes[at]!='|'){
        size_t end=at;while(end<bytes.size() && bytes[end]!='\n')++end;
        std::string line(reinterpret_cast<const char*>(bytes.data()+at),end-at);at=end<bytes.size()?end+1:end;
        if(!line.empty() && line.back()=='\r')line.pop_back();
        auto space=line.find(' ');if(space==std::string::npos)return false;
        auto key=line.substr(0,space),value=line.substr(space+1);
        if(headers.empty() && (key!="version" || value!="3"))return false;
        if(key=="comment" || key=="subtitle")continue;
        if(!headers.emplace(key,value).second)return false;
    }
    int32_t version=0,emu_version=0,port2=-1,four=0,binary=0,length=-1;
    auto required=[&](const char* k,int32_t& n){auto i=headers.find(k);return i!=headers.end() && integer(i->second,n);};
    auto optional=[&](const char* k,int32_t& n){auto i=headers.find(k);return i==headers.end() || integer(i->second,n);};
    if(!required("version",version)||version!=3 || !required("emuVersion",emu_version) ||
       !required("port2",port2)||port2!=0 || !optional("fourscore",four)||(four!=0 && four!=1) ||
       !optional("binary",binary)||(binary!=0 && binary!=1) || !optional("length",length)||length< -1 ||
       (length>=0 && size_t(length)>max_frames) || headers.count("savestate"))return false;
    std::vector<int32_t> ports;
    if(four)ports={1,1,1,1};else{int32_t p0=-1,p1=-1;if(!required("port0",p0)||!required("port1",p1)||p0<0||p0>1||p1<0||p1>1)return false;ports={p0,p1};}
    text(m.info.platform,"NES");if(headers.count("romFilename"))text(m.info.rom_name,headers["romFilename"]);
    int32_t rerecord=0;if(!optional("rerecordCount",rerecord)||rerecord<0)return false;m.info.rerecord_count=rerecord;
    auto add=[&](int32_t command,const std::vector<uint8_t>& values){
        // TASFrameData can represent soft reset, not power/FDS/VS commands.
        if(command<0 || command>1 || m.frames.size()>=max_frames)return false;
        TASFrameData f{};f.frame_number=m.frames.size();f.has_reset=command==1;
        for(size_t i=0;i<values.size();++i)f.controller_inputs[i]=buttons(values[i]);m.frames.push_back(f);return true;
    };
    if(binary){if(at==bytes.size() || bytes[at++]!='|')return false;
        while(at<bytes.size() && (length<0 || m.frames.size()<size_t(length))){
            int command=bytes[at++];std::vector<uint8_t> values;
            for(auto port:ports){if(port){if(at==bytes.size())return false;values.push_back(bytes[at++]);}else values.push_back(0);}
            if(!add(command,values))return false;
        }
    }else while(at<bytes.size() && (length<0 || m.frames.size()<size_t(length))){
        size_t end=at;while(end<bytes.size() && bytes[end]!='\n')++end;
        std::string line(reinterpret_cast<const char*>(bytes.data()+at),end-at);at=end<bytes.size()?end+1:end;
        if(!line.empty() && line.back()=='\r')line.pop_back();
        std::vector<std::string> fields;size_t start=0;
        for(size_t i=0;i<=line.size();++i)if(i==line.size() || line[i]=='|'){fields.push_back(line.substr(start,i-start));start=i+1;}
        if(fields.size()!=ports.size()+4 || !fields.front().empty() || !fields.back().empty() || !fields[ports.size()+2].empty())return false;
        int32_t command=0;if(!integer(fields[1],command))return false;std::vector<uint8_t> values;
        for(size_t i=0;i<ports.size();++i){const auto& pad=fields[i+2];if(pad.size()!=size_t(ports[i]?8:0))return false;
            uint8_t value=0;for(size_t j=0;j<pad.size();++j)if(pad[j]!='.' && pad[j]!=' ')value|=1u<<(7-j);values.push_back(value);
        }if(!add(command,values))return false;
    }
    if(length>=0 && m.frames.size()!=size_t(length))return false;
    m.info.frame_count=m.frames.size();return valid(m);
}
}
bool load_movie_file(const char* filename,Movie& movie) {
    if(!filename || !*filename)return false;
    std::ifstream file(filename,std::ios::binary|std::ios::ate);if(!file)return false;
    const auto size=file.tellg();if(size<0 || uint64_t(size)>max_file)return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(size));file.seekg(0);
    if(!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return false;
    Movie candidate;bool result=false;
    if(bytes.size()>=4 && std::memcmp(bytes.data(),"TAS2",4)==0)result=tas2(bytes,candidate);
    else if(bytes.size()>=4 && std::memcmp(bytes.data(),"TAS1",4)==0){candidate.format=MovieFormat::TAS1;result=tas1(bytes,candidate);}
    else {candidate.format=MovieFormat::FM2;result=fm2(bytes,candidate);}
    if(result)movie=std::move(candidate);return result;
}
bool save_movie_file(const char* filename,const Movie& movie) {
    if(!filename || !*filename || !valid(movie))return false;
    std::ofstream file(filename,std::ios::binary);if(!file)return false;
    auto number=[&](uint64_t n,size_t count){for(size_t i=0;i<count;++i)file.put(char((n>>(8*i))&255));};
    auto raw=[&](const auto& a){file.write(a,sizeof(a));};
    file.write("TAS2",4);raw(movie.info.title);raw(movie.info.author);raw(movie.info.description);raw(movie.info.platform);raw(movie.info.rom_name);
    number(movie.info.rom_crc32,4);number(movie.frames.size(),8);number(movie.info.rerecord_count,8);number(movie.info.starts_from_savestate,1);number(movie.start_state.size(),4);
    if(!movie.start_state.empty())file.write(reinterpret_cast<const char*>(movie.start_state.data()),movie.start_state.size());
    for(const auto& f:movie.frames){number(f.frame_number,8);for(auto input:f.controller_inputs)number(input,4);number(f.has_reset|f.has_savestate<<1,1);number(uint32_t(f.savestate_slot),4);}
    file.flush();return bool(file);
}
}
