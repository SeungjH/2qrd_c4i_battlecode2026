#include "../candidate/src/controller.hpp"
#include <cassert>
namespace battlecode {
struct BotTestAccess {
    static void prepare(Bot& b) {
        b.id=2;b.team='A';b.width=b.height=12;b.roundNo=20;b.length=2;b.units=6;b.role=4;
        b.localAt.fill(-1);
        for(int j=0;j<49;j++){b.tiles[j]=Tile{};b.tiles[j].p={j%7,j/7};b.localAt[b.code(b.tiles[j].p)]=j;}
        for(auto& row:b.he)for(auto& s:row)s=".";
        for(auto& row:b.ve)for(auto& s:row)s=".";
        b.tiles[24].id=2;b.tiles[24].team='A';b.tiles[24].head=true;
        b.tiles[31].id=2;b.tiles[31].team='A';
        b.tiles[25].id=3;b.tiles[25].team='B';b.tiles[25].head=true;
        b.tiles[23].id=4;b.tiles[23].team='A';b.tiles[23].head=true;
        b.body={b.tiles[24].p,b.tiles[31].p};
        b.huntEnemy=3;b.rlAttackFresh=true;b.rlAttackEnemy=b.tiles[25].p;b.rlAttackSlot=0;
        b.rlAttackSeen=20;b.rlAttackStarted=18;b.rlAttackPeers={4};b.rlAttackRoster={2,4};
    }
    static void setId(Bot&b,int id){b.id=id;}
    static void setRound(Bot&b,int rnd){b.roundNo=rnd;}
};
}
int main(){
    battlecode::Bot b;battlecode::BotTestAccess::prepare(b);
    battlecode::ActionPlan p;p.reason="HUNT_BLOCK";p.chosen.dir=0;p.chosen.to=17;
    auto x=battlecode::AttackRL::observation(b);
    assert(x[490]==2/12.0 && x[498]==1/8.0 && x[499]==0);
    assert(x[506]==1 && x[507]==-1/8.0 && x[511]==0 && x[512]==1 && x[513]==0);
    for(double v:x)assert(std::isfinite(v));
    assert(battlecode::AttackRL::choose(b,p));
    assert(p.reason=="RL_ATTACK" && p.chosen.dir>=0 && p.chosen.dir<4 && p.splitSize==0);
    p.reason="PEARL_ROUTE";auto d=p.chosen.dir;assert(!battlecode::AttackRL::choose(b,p) && p.chosen.dir==d);
    p.reason="HUNT_BLOCK";p.splitSize=2;assert(!battlecode::AttackRL::choose(b,p));p.splitSize=0;
    battlecode::BotTestAccess::setId(b,0);assert(!battlecode::AttackRL::choose(b,p));
    battlecode::BotTestAccess::setId(b,2);battlecode::BotTestAccess::setRound(b,350);assert(!battlecode::AttackRL::choose(b,p));
    battlecode::BotTestAccess::setRound(b,20);battlecode::AttackRL::disable(b);assert(!battlecode::AttackRL::choose(b,p));
    std::cout<<"Feature layout and attack-only override scopes passed\n";
}
