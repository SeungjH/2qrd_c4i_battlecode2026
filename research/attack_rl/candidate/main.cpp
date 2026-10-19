#include <iomanip>
#include "src/controller.hpp"
int main(int argc,char**argv) {
    if(argc>1 && std::string(argv[1])=="--rl-parity"){
        std::array<double,522>x{};for(double&v:x)std::cin>>v;
        auto y=battlecode::AttackRL::forward(x);std::cout<<std::setprecision(15);for(double v:y)std::cout<<v<<" ";return 0;
    }
    static battlecode::Bot bot;
    if(argc>1 && std::string(argv[1])=="--disable-rl")battlecode::AttackRL::disable(bot);
    return bot.run();
}
