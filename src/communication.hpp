#pragma once
#include "bot.hpp"

namespace battlecode {

inline unsigned long long Bot::encodePacket(Packet p) {
    return (0xD7ULL<<56) | (static_cast<unsigned long long>(team=='B')<<55) |
        (static_cast<unsigned long long>(p.type&7)<<52) |
        (static_cast<unsigned long long>(p.sender&65535)<<36) |
        (static_cast<unsigned long long>(p.stamp&511)<<27) | (p.payload&0x7ffffffu);
}

inline int Bot::packetPriority(int type) {
    if(type==ESCAPE) return 7;
    if(type==DANGER) return 6;
    if(type==ACCEPT) return 5;
    if(type==OFFER) return 4;
    if(type==QUEEN) return 3;
    if(type==PORTAL_ROUTE_PACKET) return 2;
    return 1;
}

inline void Bot::sendPacket(int type,unsigned payload,int stamp,bool /*urgent*/) {
    if(hasOutgoing && packetPriority(outgoing.type)>packetPriority(type)) return;
    outgoing={type,id,stamp<0?roundNo:stamp,payload}; hasOutgoing=true;
}

inline bool Bot::feedTargetFresh() { return feedTargetId>=0 && roundNo-feedTargetStamp<=policy::QueenReportAge; }

inline void Bot::selectFeedTarget() {
    int minimum=roundNo>=350?3:8;
    int incumbent=feedTargetId;
    feedTargetId=-1; feedTargetLength=0;
    for(auto it=allyReports.begin();it!=allyReports.end();) {
        if(roundNo-it->second.stamp>policy::QueenReportAge) { it=allyReports.erase(it); continue; }
        const Ally &a=it->second;
        if(a.len>=minimum && (a.len>feedTargetLength || (a.len==feedTargetLength && it->first<feedTargetId))) {
            feedTargetId=it->first;feedTargetLength=a.len;feedTargetHead=a.head;feedTargetStamp=a.stamp;
        }
        ++it;
    }
    // Finish the active gift before replacing its receiver for a tiny length lead.
    auto old=allyReports.find(incumbent);
    if(roundNo>=350 && receiveDonor>=0 && roundNo<=receiveUntil && incumbent==id &&
       old!=allyReports.end() && old->second.len>=minimum && feedTargetLength<=old->second.len+2) {
        feedTargetId=incumbent;feedTargetLength=old->second.len;feedTargetHead=old->second.head;feedTargetStamp=old->second.stamp;
    }
    if(roundNo>=350 && incumbent>=2 && old!=allyReports.end() && old->second.len>=minimum &&
       roundNo-feedTargetSince<64 && feedTargetLength<old->second.len+4) {
        feedTargetId=incumbent;feedTargetLength=old->second.len;
        feedTargetHead=old->second.head;feedTargetStamp=old->second.stamp;
    }
    // IDs 0 and 1 are fixed queens. A split child never inherits this role.
    if(officialQueenId>=0 && officialQueenDeath<0) {
        auto fixed=allyReports.find(officialQueenId);
        feedTargetId=-1;feedTargetLength=0;
        if(fixed!=allyReports.end()) {
            feedTargetId=fixed->first;feedTargetLength=fixed->second.len;
            feedTargetHead=fixed->second.head;feedTargetStamp=fixed->second.stamp;
        }
    }
    if(feedTargetId!=incumbent) feedTargetSince=roundNo;
}

inline void Bot::rememberAllyReport(int owner,int len,Pos head,int stamp) {
    if(owner<0 || len<2 || head.x<0 || head.x>=width || head.y<0 || head.y>=height ||
       stamp>roundNo || roundNo-stamp>policy::QueenReportAge) return;
    auto it=allyReports.find(owner);
    if(it!=allyReports.end() && stamp<it->second.stamp) return;
    if(it!=allyReports.end() && len<it->second.len &&
       (owner==feedTargetId || (owner==id && it->second.len>=8))) {
        retiredQueen={QUEEN,owner,stamp,unsigned(code(head))|(unsigned(len)<<12)};
        retiredUntil=roundNo+12;
    }
    if(owner<=1) {
        if(officialQueenDeath>=stamp) return;
        officialQueenId=owner;officialQueenDeath=-1;
    }
    allyReports[owner]={len,stamp,0,head};
    selectFeedTarget();
}

inline bool Bot::reservedPearl(int key) {
    // A visible fixed queen reserves nearby food even when its SONAR beam missed us.
    if(roundNo>=policy::SupportStart && !officialQueen()) for(const Tile &t:tiles)
        if(t.head && t.team==team && t.id<=1 && t.id>=0 && distance(decodePos(key),t.p)<=4) return true;
    if(roundNo>=policy::SupportStart && feedTargetFresh() && feedTargetId>=0 && feedTargetId<=1 && feedTargetId!=id) {
        auto home=rallyReports.find(feedTargetId);
        if(home!=rallyReports.end() && roundNo-home->second.second<=20 &&
           homeDistance(decodePos(key),home->second.first)<=max(4,feedTargetLength/4+2)) return true;
    }
    auto it=reservedFood.find(key);
    if(it!=reservedFood.end() && it->second.second>=roundNo && it->second.first!=id) return true;
    return roundNo>=280 && feedTargetFresh() && feedTargetId!=id && roundNo-feedTargetStamp<=8 &&
        distance(decodePos(key),feedTargetHead)<=4;
}

inline bool Bot::exclusivePearl(int key) {
    auto gift=reservedFood.find(key);
    if(gift!=reservedFood.end() && gift->second.second>=roundNo && gift->second.first!=id) return true;
    if(roundNo>=policy::SupportStart && !officialQueen()) for(const Tile &t:tiles)
        if(t.head && t.team==team && t.id>=0 && t.id<=1 && distance(decodePos(key),t.p)<=4) return true;
    return feedTargetFresh() && feedTargetId>=0 && feedTargetId<=1 && feedTargetId!=id && reservedPearl(key);
}

inline void Bot::rememberResource(int key,int score,int stamp,bool fountain) {
    if(!validKey(key) || stamp>roundNo || roundNo-stamp>(fountain?60:24)) return;
    auto &r=resources[key];
    if(stamp>=r.stamp) { r.stamp=stamp;r.score=score;r.fountain=fountain; }
    if(resources.size()>32) {
        auto old=min_element(resources.begin(),resources.end(),[](const auto&a,const auto&b){
            return a.second.stamp+a.second.score*2<b.second.stamp+b.second.score*2;
        });
        resources.erase(old);
    }
}

inline void Bot::addWarning(int entrance,int d,int stamp,bool permanent,bool localProof) {
    if(!validKey(entrance)) return;
    int expiry=permanent?501:stamp+20;
    (void)localProof;
    if(expiry<roundNo) return;
    int k=entrance*4+d;
    auto it=warnings.find(k);
    if(it==warnings.end() || expiry>it->second.expiry)
        warnings[k]={stamp,expiry,-100,permanent,++warningSequence};
    if(warnings.size()>64) {
        // Keep permanent warnings until all temporary warnings have been evicted.
        auto oldest=min_element(warnings.begin(),warnings.end(),[](const auto&a,const auto&b){
            if(a.second.permanent!=b.second.permanent) return !a.second.permanent;
            return a.second.sequence<b.second.sequence;
        });
        warnings.erase(oldest);
    }
}

inline bool Bot::avoided(int k,int d) {
    auto it=warnings.find(k*4+d);
    return it!=warnings.end() && it->second.expiry>=roundNo;
}

inline void Bot::receiveRadio() {
    received.clear();
    for(auto raw:inbox) {
        if((raw>>56)!=0xD7 || int((raw>>55)&1)!=(team=='B')) continue;
        Packet p{int((raw>>52)&7),int((raw>>36)&65535),int((raw>>27)&511),unsigned(raw&0x7ffffff)};
        if(p.sender==id || p.stamp>roundNo || p.type>QUEEN) continue;
        int age=roundNo-p.stamp;
        if(p.type==QUEEN) {
            if(p.payload&(1u<<26)) {
                int key=p.payload&4095;
                if(validKey(key) && age<=20) {
                    auto old=rallyReports.find(p.sender);
                    if(old==rallyReports.end() || p.stamp>=old->second.second)
                        rallyReports[p.sender]={key,p.stamp};
                    if(rallyReports.size()>32) rallyReports.erase(rallyReports.begin());
                }
                continue;
            }
            int key=p.payload&4095,len=(p.payload>>12)&4095;
            if(len==0 && p.sender<=1 && (officialQueenId<0 || p.sender==officialQueenId) && age<=policy::QueenReportAge &&
               (!allyReports.count(p.sender) || p.stamp>=allyReports[p.sender].stamp)) {
                officialQueenId=p.sender;officialQueenDeath=p.stamp;allyReports.erase(p.sender);selectFeedTarget();
                retiredQueen=p;retiredUntil=roundNo+12;
            }
            if(validKey(key) && len>=2) rememberAllyReport(p.sender,len,decodePos(key),p.stamp);
            continue;
        }
        if(p.type==DANGER) {
            addWarning(p.payload&4095,(p.payload>>12)&3,p.stamp,(p.payload>>14)&1);
            continue;
        }
        if(p.type==PORTAL_ROUTE_PACKET) {
            // Bit 26 distinguishes resource reports from immutable portal routes.
            if(p.payload&(1u<<26)) {
                rememberResource(p.payload&4095,(p.payload>>12)&15,p.stamp,(p.payload>>16)&1);
                continue;
            }
            if(age>8) continue;
            int entry=p.payload&4095,d=(p.payload>>12)&3,exit=(p.payload>>14)&4095;
            if(!validKey(entry) || !validKey(exit) || sharedPortalExit.size()>=128) continue;
            int key=entry*4+d;
            if(!sharedPortalExit.count(key)) {
                sharedPortalExit[key]=exit;
                if(terrain[entry][d]<0) terrain[entry][d]=exit;
            }
            continue;
        }
        if(age>4) continue;
        if(p.type==HUNT && !(p.payload&(1u<<26)) && turnsAlive==0 && length==2 &&
           p.sender<id && int(p.payload&4095)==code(tiles[24].p) && visibleAlly(p.sender)) {
            attackChildEnemy=(p.payload>>12)&4095;attackChildUntil=roundNo+2;
            courierUntil=roundNo-1;
            continue;
        }
        if(p.type==ESCAPE) {
            int child=p.payload&4095,entry=(p.payload>>12)&4095;
            // A proactive branch assignment uses reserved entry 4094, not a danger warning.
            if(turnsAlive==0 && p.sender<id && child==code(tiles[24].p) && entry==4094 && length==2) {
                bearing=(p.payload>>24)&3;reverseChildUntil=roundNo;courierUntil=roundNo-1;
                continue;
            }
            // Reserved entry 4095 tells the reversed child to leave; it does not transfer queen identity.
            if(turnsAlive<=1 && p.sender<id && child==code(tiles[24].p) && entry==4095 && length>=2) {
                reverseChildUntil=roundNo+2;bearing=facing;
                continue;
            }
            if(turnsAlive<=1 && p.sender<id && child==code(tiles[24].p) && validKey(entry)) {
                courierDir=(p.payload>>24)&3;
                courierPermanent=(p.payload>>26)&1; courierStamp=p.stamp;
                addWarning(entry,courierDir,p.stamp,courierPermanent);
                if(length<=3) { courierEntry=entry; courierUntil=roundNo+32; }
                else { reverseChildUntil=roundNo+8; bearing=facing; }
            }
            continue;
        }
        if(!visibleAlly(p.sender)) continue;
        if(p.type==STATUS) {
            int key=p.payload&4095,len=(p.payload>>12)&4095;
            if(!validKey(key) || len<2) continue;
            int h=visibleHead(p.sender);
            // A status reports post-action position. Allow the few turns since it was sent.
            if(h>=0 && distance(decodePos(key),tiles[h].p)>age+1) continue;
            allies[p.sender]={len,p.stamp,int((p.payload>>24)&7),decodePos(key)};
            if(p.sender==feedTargetId && len<(roundNo>=350?3:8)) feedTargetId=-1;
            rememberAllyReport(p.sender,len,decodePos(key),p.stamp);
        } else {
            received.push_back(p);
            if(p.type==OFFER) {
                int receiver=p.payload&65535;
                for(const Tile &t:tiles) if(t.id==p.sender && t.team==team)
                    reservedFood[code(t.p)]={receiver,roundNo+10};
            }
            if(p.type==ACCEPT && p.sender==feedTargetId) {
                int donor=p.payload&65535;servingDonor=donor;servingUntil=roundNo+6;
                for(const Tile &t:tiles) if(t.id==donor && t.team==team)
                    reservedFood[code(t.p)]={p.sender,roundNo+10};
            }
        }
    }
    for(auto it=allies.begin();it!=allies.end();) {
        if(roundNo-it->second.stamp>8) it=allies.erase(it); else ++it;
    }
    for(auto it=warnings.begin();it!=warnings.end();) {
        if(it->second.expiry<roundNo) it=warnings.erase(it); else ++it;
    }
    // Our queens move one tile without portals. Only certify death when every
    // possible next head position fits inside this observation, not on a timeout.
    if(officialQueenId>=0 && officialQueenDeath<0 && feedTargetId==officialQueenId &&
       feedTargetId!=id && distance(tiles[24].p,feedTargetHead)+max(1,roundNo-feedTargetStamp+1)<=3 &&
       !visibleAlly(officialQueenId)) {
        officialQueenDeath=roundNo;allyReports.erase(officialQueenId);
        retiredQueen={QUEEN,officialQueenId,roundNo,unsigned(code(feedTargetHead))};
        retiredUntil=roundNo+12;selectFeedTarget();
    }
    if(!feedTargetFresh()) feedTargetId=-1;
    for(const Tile &t:tiles) if(t.head && t.team==team && t.id!=id) {
        auto report=allyReports.find(t.id);
        if(report==allyReports.end() || roundNo-report->second.stamp>8) continue;
        auto previous=allies.find(t.id);
        int observedRole=previous==allies.end()?0:previous->second.role;
        allies[t.id]={report->second.len,roundNo,observedRole,t.p};
        report->second.head=t.p;
        if(t.id==feedTargetId) feedTargetHead=t.p;
    }
    rememberAllyReport(id,length,tiles[24].p,roundNo);
    for(auto it=reservedFood.begin();it!=reservedFood.end();)
        if(it->second.second<roundNo) it=reservedFood.erase(it); else ++it;
    for(auto it=resources.begin();it!=resources.end();)
        if(roundNo-it->second.stamp>(it->second.fountain?60:24)) it=resources.erase(it); else ++it;
}

inline void Bot::emitRadio() {
    int postLength=int(body.size());
    if(postLength<2 || (!body.empty() && body.front().x<0)) postLength=length;
    Pos head=body.empty()?tiles[24].p:body.front();
    if(head.x<0) head=tiles[24].p;
    rememberAllyReport(id,postLength,head,roundNo);
    Packet status{STATUS,id,roundNo,unsigned(code(head))|(unsigned(min(postLength,4095))<<12)|(unsigned(role)<<24)};
    if(!hasOutgoing && !warnings.empty() && roundNo%3==0) {
        auto it=min_element(warnings.begin(),warnings.end(),[](const auto&a,const auto&b){return a.second.lastSent<b.second.lastSent;});
        if(roundNo-it->second.lastSent>=4) {
            sendPacket(DANGER,unsigned(it->first/4)|(unsigned(it->first%4)<<12)|
                (unsigned(it->second.permanent)<<14),it->second.stamp);
            it->second.lastSent=roundNo;
        }
    }
    Packet queen;bool shareQueen=false;
    if(roundNo<=retiredUntil && (retiredQueen.stamp==roundNo || roundNo%3==1)) {queen=retiredQueen;shareQueen=true;}
    else if(feedTargetFresh() && (feedTargetId<=1 || feedTargetLength>=(roundNo>=350?3:8))) {
        queen={QUEEN,feedTargetId,feedTargetStamp,unsigned(code(feedTargetHead))|(unsigned(min(feedTargetLength,4095))<<12)};
        shareQueen=true;
    }
    Packet info;bool shareInfo=false;
    int resource=-1,oldest=10000;
    for(auto [key,r]:resources) if(roundNo-r.lastSent>=8 && r.lastSent<oldest) {resource=key;oldest=r.lastSent;}
    if(resource>=0 && (roundNo%2==0 || sharedPortalExit.empty())) {
        auto &r=resources[resource];
        info={PORTAL_ROUTE_PACKET,id,r.stamp,unsigned(resource)|(unsigned(r.score)<<12)|
            (unsigned(r.fountain)<<16)|(1u<<26)};
        r.lastSent=roundNo;shareInfo=true;
    } else {
        int key=-1;oldest=10000;
        for(auto [k,exit]:sharedPortalExit) {
            (void)exit;int sent=portalRouteLastSent.count(k)?portalRouteLastSent[k]:-100;
            if(roundNo-sent>=8 && sent<oldest) {oldest=sent;key=k;}
        }
        if(key>=0) {
            info={PORTAL_ROUTE_PACKET,id,roundNo,unsigned(key/4)|(unsigned(key%4)<<12)|
                (unsigned(sharedPortalExit[key])<<14)};
            portalRouteLastSent[key]=roundNo;shareInfo=true;
        }
    }
    if(feedTargetId==id && assemblyCenter>=0) rallyReports[id]={assemblyCenter,roundNo};
    auto rally=rallyReports.find(feedTargetId);
    if(feedTargetFresh() && rally!=rallyReports.end() && roundNo-rally->second.second<=20 && roundNo%2==0) {
        info={QUEEN,feedTargetId,rally->second.second,unsigned(rally->second.first)|
            (unsigned(min(feedTargetLength,4095))<<12)|(1u<<26)};shareInfo=true;
    }
    int aligned=-1;
    if(hasOutgoing && (outgoing.type==OFFER || outgoing.type==ACCEPT)) {
        int owner=outgoing.type==OFFER?donationTarget:receiveDonor,at=visibleHead(owner);
        if(at>=0) {
            Pos other=tiles[at].p;
            if(head.x==other.x && head.y!=other.y)
                aligned=wrap(other.y-head.y,height)<=wrap(head.y-other.y,height)?2:0;
            else if(head.y==other.y && head.x!=other.x)
                aligned=wrap(other.x-head.x,width)<=wrap(head.x-other.x,width)?1:3;
        }
    }
    int queenDir=(roundNo+id)%4;
    if(queenDir==aligned) queenDir=(queenDir+1)%4;
    int infoDir=(queenDir+1)%4;
    if(infoDir==aligned) infoDir=(infoDir+1)%4;
    Packet primary=outgoing;bool tactic=hasOutgoing;
    if(!hasOutgoing && shareQueen) {outgoing=queen;hasOutgoing=true;queenLastSent=roundNo;}
    else if(!hasOutgoing && shareInfo) {outgoing=info;hasOutgoing=true;}
    for(int d=0;d<4;d++) {
        Packet packet=status;
        if(tactic && (primary.type==ESCAPE || attackSplitRadio || d==aligned || ((roundNo+d)%2==0))) packet=primary;
        // Give length/election reports a channel even while warnings or offers are active.
        if(shareQueen && d==queenDir && (primary.type!=ESCAPE || roundNo<=retiredUntil)) packet=queen;
        if(shareInfo && d==infoDir && (!tactic || (primary.type!=ESCAPE && d!=aligned))) packet=info;
        // Receiver pulses break the direction/position phase lock of a short home circuit.
        if(shareQueen && feedTargetId==id && !tactic && roundNo%3==0)
            packet=(shareInfo && d==infoDir)?info:queen;
        cout<<"SONAR "<<DIRS[d]<<' '<<encodePacket(packet)<<'\n';
    }
}

} // namespace battlecode
