#include "../candidate/src/controller.hpp"
#include <cassert>
namespace battlecode {
struct BotTestAccess {
 static void setup(Bot&b) {
  b.id=2;b.team='A';b.width=b.height=12;b.roundNo=20;b.length=2;b.units=6;b.role=0;
  b.localAt.fill(-1);b.lastVisit.fill(-10000);b.spawnOneRound.fill(-100);b.harvestRound.fill(-100);
  for(auto&r:b.terrain)r.fill(-2);for(auto&r:b.portalAt)r.fill(-1);
  for(int j=0;j<49;j++){b.tiles[j]=Tile{};b.tiles[j].p={j%7,j/7};b.localAt[b.code(b.tiles[j].p)]=j;}
  for(auto&r:b.he)for(auto&s:r)s=".";for(auto&r:b.ve)for(auto&s:r)s=".";
  for(int j:{24,31}){b.tiles[j].id=2;b.tiles[j].team='A';}b.tiles[24].head=true;
  for(int j:{23,22}){b.tiles[j].id=4;b.tiles[j].team='A';}b.tiles[23].head=true;
  for(int j:{27,34}){b.tiles[j].id=3;b.tiles[j].team='B';}b.tiles[27].head=true;
  b.body={b.tiles[24].p,b.tiles[31].p};Ally a;a.len=2;a.stamp=20;a.role=0;a.head=b.tiles[23].p;b.allies[4]=a;
 }
 static bool hunt(Bot&b){b.role=0;b.rlAttackFresh=false;auto moves=b.legalMoves();return b.coordinateHunt(moves);}
 static void hidePeer(Bot&b){for(Tile&t:b.tiles)if(t.id==4){t.id=-1;t.head=false;t.team=0;}}
 static void round(Bot&b,int r){b.roundNo=r;}
 static int target(Bot&b){return b.huntEnemy;}
 static bool fresh(Bot&b){return b.rlAttackFresh;}
 static void queen(Bot&b){b.id=0;}
 static void grow(Bot&b){b.length=5;}
};
}
int main(){using namespace battlecode;
 Bot old;BotTestAccess::setup(old);AttackRL::disable(old);assert(!BotTestAccess::hunt(old));
 Bot b;BotTestAccess::setup(b);assert(BotTestAccess::hunt(b));assert(BotTestAccess::target(b)==3 && BotTestAccess::fresh(b));
 BotTestAccess::hidePeer(b);BotTestAccess::round(b,21);assert(BotTestAccess::hunt(b));
 BotTestAccess::round(b,23);assert(BotTestAccess::hunt(b));BotTestAccess::round(b,24);assert(!BotTestAccess::hunt(b));
 Bot q;BotTestAccess::setup(q);BotTestAccess::queen(q);assert(!BotTestAccess::hunt(q));
 Bot longBot;BotTestAccess::setup(longBot);BotTestAccess::grow(longBot);assert(!BotTestAccess::hunt(longBot));
 Bot late;BotTestAccess::setup(late);BotTestAccess::round(late,350);assert(!BotTestAccess::hunt(late));
 std::cout<<"Open short-target entry, peer grace and protected scopes passed\n";
}
