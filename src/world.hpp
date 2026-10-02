#pragma once
#include "bot.hpp"

namespace battlecode {

inline const string& Bot::edge(int at,int d) {
    int r=at/7,c=at%7;
    if(d==0) return he[r][c];
    if(d==1) return ve[r][c+1];
    if(d==2) return he[r+1][c];
    return ve[r][c];
}

inline void Bot::remember(int portalId,bool horizontal,Pos p) {
    p.x=wrap(p.x,width); p.y=wrap(p.y,height);
    Portal e{horizontal,p}; auto &pair=portalPairs[portalId];
    if(find(pair.begin(),pair.end(),e)==pair.end() && pair.size()<2) pair.push_back(e);
}

inline int Bot::portalDestination(int from,int d,int portalId) {
    auto it=portalPairs.find(portalId);
    if(it==portalPairs.end() || it->second.size()!=2) {
        auto known=sharedPortalExit.find(from*4+d);
        return known==sharedPortalExit.end()?-1:known->second;
    }
    Pos p=decodePos(from);
    Portal here{d%2==0,{wrap(p.x+(d==1),width),wrap(p.y+(d==2),height)}};
    const auto &pair=it->second;
    if(!(here==pair[0]) && !(here==pair[1])) return -1;
    Pos exit=pair[here==pair[0]?1:0].p;
    exit.x=wrap(exit.x-(d==3),width); exit.y=wrap(exit.y-(d==0),height);
    return code(exit);
}

inline bool Bot::knownPortalTrap(int destination) {
    if(destination<0 || !validKey(destination)) return false;
    int open=0,portals=0;
    for(int d=0;d<4;d++) {
        if(terrain[destination][d]==-2) return false;
        if(portalAt[destination][d]>=0) portals++;
        else if(terrain[destination][d]>=0) open++;
    }
    return open==0 && portals<=1;
}

inline bool Bot::portalReady(int portalId) {
    auto it=portalLastUsed.find(portalId);
    return it==portalLastUsed.end() || roundNo-it->second>=policy::PortalRetryDelay;
}

// Physical transition, independent of occupancy. The local destination is
// unavailable when its paired exit lies outside the vision square.
inline int Bot::nextTile(int at,int d,bool respectCooldown) {
    const string &e=edge(at,d);
    if(e=="w") return -1;
    Pos p=tiles[at].p;
    if(e==".") return local({wrap(p.x+DC[d],width),wrap(p.y+DR[d],height)});
    int portalId=stoi(e);
    if(respectCooldown && !portalReady(portalId)) return -1;
    int destination=portalDestination(code(p),d,portalId);
    return destination<0?-1:local(decodePos(destination));
}

inline void Bot::syncBody() {
    // A portal can hide the new head while the known rear remains intact.
    if(!body.empty() && body[0].x>=0 && !(body[0]==tiles[24].p))
        body.clear();
    body.resize(length);
    body[0]=tiles[24].p;
    array<bool,49> used{}; used[24]=true;
    int predecessor=24;
    for(int k=1;k<length;k++) {
        int found=-1;
        for(int j=0;j<49;j++) if(!used[j] && tiles[j].id==id && !tiles[j].head &&
            nextTile(j,tiles[j].dir,false)==predecessor) { found=j; break; }
        if(found<0) break;
        if(body[k].x>=0 && !(body[k]==tiles[found].p))
            fill(body.begin()+k,body.end(),Pos{});
        body[k]=tiles[found].p; used[found]=true; predecessor=found;
    }
}

// Keep a destination across turns, so a corridor detour is not abandoned at each bend.
inline void Bot::rememberTerrain() {
    for(int j=0;j<49;j++) {
        int k=code(tiles[j].p);
        pearlSeen[k]=tiles[j].pearl?roundNo:-1;
        if(tiles[j].countdown==1) {
            if(spawnOneRound[k]==roundNo-1) rememberResource(k,15,roundNo,true);
            spawnOneRound[k]=roundNo;
        }
        for(int d=0;d<4;d++) {
            const string &e=edge(j,d);
            if(e==".") {
                terrain[k][d]=code({wrap(tiles[j].p.x+DC[d],width),
                    wrap(tiles[j].p.y+DR[d],height)});
            } else if(e=="w") terrain[k][d]=-1;
            else {
                if(portalAt[k][d]<0) knownPortalEdges.push_back({k,d});
                portalAt[k][d]=stoi(e);
                terrain[k][d]=-3; // Observed entry, paired exit not known yet.
            }
        }
    }
    for(auto [k,d]:knownPortalEdges) {
        int destination=portalDestination(k,d,portalAt[k][d]);
        terrain[k][d]=destination>=0?destination:-3;
    }
}

inline int Bot::terrainDegree(int k) {
    int n=0; for(int to:terrain[k]) if(to>=0) n++; return n;
}

inline void Bot::recordMove(const Move &m) {
    Pos origin=body.empty() || body.front().x<0?tiles[24].p:body.front();
    previousEntry=code(origin);previousDirection=m.dir;
    if(origin==tiles[24].p) updateCorridor(m);
    if(m.portal) {
        portalUses++;
        portalLastUsed[m.portalId]=roundNo;
        lastPortalEntry=code(tiles[24].p);
        lastPortalDir=m.dir;
        lastPortalId=m.portalId;
        lastPortalRound=roundNo;
    }
    if(m.to<0) {
        bearing=m.dir;
        // Preserve the rear through an unobserved portal. Growth is reconciled
        // against next turn's length, so keep the old tail until observation.
        body.insert(body.begin(),Pos{});
        return;
    }
    int originLocal=local(origin);
    int oldForward=originLocal<0?-1:nextTile(originLocal,bearing);
    // A sustained obstacle, explored strip, or nearby ally gives the next heading.
    if(m.dir!=bearing && (oldForward<0 || tiles[oldForward].id>=0 ||
        visits[code(tiles[oldForward].p)]>0 || separation(m.to)>separation(oldForward)+3)) bearing=m.dir;
    body.insert(body.begin(),tiles[m.to].p);
    if(!tiles[m.to].pearl) body.pop_back();
    else {
        foodRounds.push_back(roundNo);
        int key=code(tiles[m.to].p);
        if(roundNo-harvestRound[key]>24) harvestCount[key]=0;
        harvestRound[key]=roundNo;harvestCount[key]++;
        if(tiles[m.to].countdown>=0 && harvestCount[key]>=2)
            rememberResource(key,min(14,harvestCount[key]*3),roundNo,false);
    }
    while(!foodRounds.empty() && roundNo-foodRounds.front()>32) foodRounds.pop_front();
}

inline void Bot::observePortalArrival() {
    if(lastPortalEntry<0 || roundNo!=lastPortalRound+1) return;
    int ordinary=0,otherPortals=0;
    for(int d=0;d<4;d++) {
        const string &e=edge(24,d);
        ordinary+=e==".";
        otherPortals+=e!="." && e!="w" && stoi(e)!=lastPortalId;
    }
    int key=lastPortalEntry*4+lastPortalDir;
    if(ordinary==0 && otherPortals==0) {
        addWarning(lastPortalEntry,lastPortalDir,roundNo,true,true);
        sendPacket(DANGER,unsigned(lastPortalEntry)|
            (unsigned(lastPortalDir)<<12)|(1u<<14),roundNo,true);
    } else if(sharedPortalExit.size()<128) {
        int exit=code(tiles[24].p);
        sharedPortalExit[key]=exit;
        terrain[lastPortalEntry][lastPortalDir]=exit;
        portalRouteLastSent[key]=roundNo;
        sendPacket(PORTAL_ROUTE_PACKET,unsigned(lastPortalEntry)|
            (unsigned(lastPortalDir)<<12)|(unsigned(exit)<<14),roundNo,true);
    }
    lastPortalEntry=-1;
}

} // namespace battlecode
