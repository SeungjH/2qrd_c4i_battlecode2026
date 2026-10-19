#pragma once
#include "bot.hpp"

namespace battlecode {

inline bool Bot::readTurn() {
    string tag; if(!(cin>>tag) || tag=="ENDGAME") return false;
    if(tag!="ROUND") return false;
    char f; int messages=0;
    if(!(cin>>roundNo>>tag>>f>>tag>>length>>tag>>units>>tag>>messages)) return false;
    facing=dirOf(f); inbox.clear();
    for(int i=0;i<messages;i++) { unsigned long long raw; if(!(cin>>raw)) return false; inbox.push_back(raw); }
    string first; if(!(cin>>first)) return false;
    if(first=="ECHOES") { int ignored; for(int i=0;i<5;i++) cin>>ignored; cin>>first; }
    localAt.fill(-1);
    for(int j=0;j<49;j++) {
        tiles[j]=Tile{};
        if(j==0) tiles[j].p.x=stoi(first); else cin>>tiles[j].p.x;
        cin>>tiles[j].p.y>>tiles[j].pearl>>tiles[j].countdown;
        localAt[code(tiles[j].p)]=j; lastSeen[code(tiles[j].p)]=roundNo;
    }
    int parts=0; cin>>tag>>parts;
    for(int i=0;i<parts;i++) {
        char t,d; int owner,x,y,head; cin>>t>>owner>>x>>y>>d>>head;
        int j=local({x,y});
        if(j>=0) { tiles[j].team=t; tiles[j].id=owner; tiles[j].head=head; tiles[j].dir=dirOf(d); }
    }
    for(int r=0;r<8;r++) for(int c=0;c<7;c++) {
        cin>>he[r][c];
        if(he[r][c]!="." && he[r][c]!="w") {
            Pos p=tiles[min(r,6)*7+c].p; p.y+=(r==7); remember(stoi(he[r][c]),true,p);
        }
    }
    for(int r=0;r<7;r++) for(int c=0;c<8;c++) {
        cin>>ve[r][c];
        if(ve[r][c]!="." && ve[r][c]!="w") {
            Pos p=tiles[r*7+min(c,6)].p; p.x+=(c==7); remember(stoi(ve[r][c]),false,p);
        }
    }
    if(!cin) return false;
    for(const Tile &t:tiles) if(t.id>=0 && t.team!=team) lastEnemySeen=roundNo;
    syncBody();
    rememberTerrain();
    receiveRadio();
    if(bearing<0) { bearing=facing; hand=((unsigned(id)*2654435761u+unsigned(width*height))&1)?1:-1; }
    return true;
}

} // namespace battlecode
