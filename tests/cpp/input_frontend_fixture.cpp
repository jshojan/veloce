#include "emu/input_plugin.hpp"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
static void log(const std::string& text){if(auto* path=std::getenv("VELOCE_INPUT_CAPTURE"))std::ofstream(path,std::ios::app)<<text<<'\n';}
class Input final:public emu::IInputPlugin{
    emu::IInputHost* host=nullptr;
public:
    emu::InputPluginInfo get_info() override{return {"Capture Input","1","Veloce","Test fixture",false,false,false};}
    bool initialize(emu::IInputHost* h) override{host=h;log("initialize");return !std::getenv("VELOCE_INPUT_REJECT");}
    void shutdown() override{log("shutdown");host=nullptr;}~Input() override{log("destroy");}
    void set_controller_layout(const emu::ControllerLayoutInfo* layout) override{if(layout)log(std::string("layout ")+layout->platform_name);}
    void begin_frame() override{
        if(!host || host->get_controller_count()!=2 || std::string(host->get_platform_name())!="Input Fixture")throw std::runtime_error("input host metadata mismatch");
        if(host->is_key_pressed(-1) || host->is_gamepad_button_pressed(-1,0) || host->get_gamepad_axis(-1,0)!=0 || host->get_connected_controller_count()!=0 || host->get_controller_info(-1).is_connected)throw std::runtime_error("input host raw polling mismatch");
        log("begin "+std::to_string(host->get_frame_number()));
    }
    uint32_t get_input_state(int controller) override{const uint32_t masks[]={1,2,1u<<21};auto value=controller==0?masks[host->get_frame_number()%3]:2048;log("sample "+std::to_string(controller)+" "+std::to_string(value));return value;}
    emu::InputBinding get_binding(int,emu::VirtualButton) const override{return {};}
    void set_binding(int,emu::VirtualButton,const emu::InputBinding&) override{}
};
extern "C" {
EMU_PLUGIN_EXPORT emu::IInputPlugin* create_input_plugin(){return new Input;}
EMU_PLUGIN_EXPORT void destroy_input_plugin(emu::IInputPlugin* p){delete p;}
EMU_PLUGIN_EXPORT uint32_t get_input_plugin_api_version(){return EMU_INPUT_PLUGIN_API_VERSION;}
}
