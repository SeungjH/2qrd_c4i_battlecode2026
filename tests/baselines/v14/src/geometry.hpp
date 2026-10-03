#pragma once
#include "bot.hpp"

namespace battlecode {

inline int Bot::wrap(int n,int m) { return (n%m+m)%m; }

inline int Bot::code(Pos p) { return p.y*64+p.x; }

inline int Bot::dirOf(char c) { for(int d=0;d<4;d++) if(DIRS[d]==c) return d; return 0; }

inline int Bot::local(Pos p) { return p.x<0?-1:localAt[code(p)]; }

inline int Bot::distance(Pos a,Pos b) {
    int x=abs(a.x-b.x),y=abs(a.y-b.y);
    return min(x,width-x)+min(y,height-y);
}

inline int Bot::homeDistance(Pos p,int home) {
    int x=abs(p.x-home%64),y=abs(p.y-home/64);
    return max(min(x,width-x),min(y,height-y));
}

inline Pos Bot::decodePos(int k) { return {k%64,k/64}; }

inline bool Bot::validKey(int k) { Pos p=decodePos(k); return p.x<width && p.y<height; }

inline int Bot::visibleHead(int owner) {
    for(int j=0;j<49;j++) if(tiles[j].id==owner && tiles[j].head) return j;
    return -1;
}

inline bool Bot::visibleAlly(int owner) {
    for(const auto &t:tiles) if(t.id==owner && t.team==team) return true;
    return false;
}

inline bool Bot::freshAlly(int owner) {
    auto it=allies.find(owner);
    return it!=allies.end() && roundNo-it->second.stamp<=4 && visibleAlly(owner);
}

} // namespace battlecode
