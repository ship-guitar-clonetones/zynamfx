#include "../plugin/ui/widgets/HostedEffectWindow.cpp"
#include <iostream>
class Smoke : public juce::JUCEApplication, private juce::Timer {
public:
 const juce::String getApplicationName() override {return "zynamfx editor smoke";}
 const juce::String getApplicationVersion() override {return "1";}
 void initialise(const juce::String&) override {
  juce::AudioUnitPluginFormat format;
  juce::OwnedArray<juce::PluginDescription> descriptions;
  format.findAllTypesForFile(descriptions, "/Library/Audio/Plug-Ins/Components/ATTRACTOR.component");
  if(descriptions.isEmpty()){std::cerr<<"ATTRACTOR not found\n";setApplicationReturnValue(1);quit();return;}
  juce::String error;
  auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,128,error);
  if(!plugin){std::cerr<<error<<"\n";setApplicationReturnValue(2);quit();return;}
  instance=std::move(plugin);
  instance->prepareToPlay(48000,128);
  open();startTimer(1000);
 }
 void shutdown() override {window.reset();if(instance)instance->releaseResources();instance.reset();}
private:
 void open(){window=std::make_unique<t3k::ui::HostedEffectWindow>(instance,"ATTRACTOR smoke","smoke");}
 void timerCallback() override {
  window.reset();std::cout<<"ATTRACTOR editor open/close cycle "<<++cycles<<" passed"<<std::endl;
  if(cycles==5){stopTimer();quit();}else open();
 }
 std::shared_ptr<juce::AudioPluginInstance> instance;
 std::unique_ptr<t3k::ui::HostedEffectWindow> window;
 int cycles=0;
};
START_JUCE_APPLICATION(Smoke)
