#pragma once
#include "bot.hpp"

namespace battlecode {

inline vector<int> Bot::visibleBody(int owner,int expected) {
    vector<int> out; int at=visibleHead(owner); array<bool,49> used{};
    if(at<0 || expected>49) return out;
    out.push_back(at); used[at]=true;
    while(int(out.size())<expected) {
        int found=-1;
        for(int j=0;j<49;j++) if(!used[j] && tiles[j].id==owner &&
            nextTile(j,tiles[j].dir,false)==at) { found=j; break; }
        if(found<0) return out; // A visible prefix is enough to plan a partial gift.
        out.push_back(found); used[found]=true; at=found;
    }
    return out;
}

inline Collection Bot::collectionPlan(int donor,int receiver,int receiverLength) {
    Collection plan;
    int receiverHead=visibleHead(receiver);
    int donorLength=donor==id?length:(freshAlly(donor)?allies[donor].len:0);
    int cap=officialQueenDeath>=0 && roundNo>=400?24:12;
    if(receiverHead<0 || donorLength<2 || donorLength>cap) return plan;
    auto parts=visibleBody(donor,donorLength);
    if(donor==id && int(body.size())==length) {
        parts.clear();
        for(Pos p:body) parts.push_back(local(p));
    }
    if(parts.empty()) return plan;
    array<bool,49> blocked{};
    for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0 && tiles[j].id!=donor;
    Region r=region(receiverHead,blocked,false);
    for(int k=0;k<int(parts.size());k+=2) {
        int at=parts[k];
        if(at<0) continue;
        if(r.dist[at]<0) continue;
        plan.count++;
        if(r.dist[at]<plan.dist) { plan.dist=r.dist[at]; plan.closest=at; }
    }
    int expected=(donorLength+1)/2;
    int required=roundNo<400 && !quietEndgame()?expected:max(1,(expected+1)/2);
    if(plan.closest<0 || plan.dist>5 || plan.count<required ||
       roundNo+plan.dist+plan.count*2+2>=500) return plan;
    if(!r.frontier && r.area<receiverLength+plan.count+3) return plan;
    int turns=0;for(int d=0;d<4;d++) {int n=nextTile(receiverHead,d,false);if(n>=0 && !blocked[n]) turns++;}
    // One forward exit is normal in a corridor. Keep room and enemy checks;
    // requiring two immediate exits prevented safe gifts beside moving receivers.
    if(turns<1) return plan;
    // Don't donate food in a contested pocket; this is intentionally conservative.
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team!=team &&
        distance(tiles[j].p,tiles[plan.closest].p)<=plan.dist+2) return plan;
    plan.safe=true; return plan;
}

inline int Bot::deliberateDeathDirection() {
    for(int d=0;d<4;d++) if(edge(24,d)=="w") return d;
    for(int d=0;d<4;d++) {
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id==id && !tiles[to].head) return d;
    }
    for(int d=0;d<4;d++) {
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id>=0 && tiles[to].team==team && !tiles[to].head) return d;
    }
    return -1;
}

inline bool Bot::betterRank(int owner,int len) { return len>length || (len==length && owner<id); }

inline bool Bot::approachAlly(vector<Move>& moves,int owner) {
    int h=visibleHead(owner); if(h<0) return false;
    Pos meeting=tiles[h].p;
    int forward=tiles[h].dir,at=code(meeting);
    for(int step=0;step<2;step++) {
        int next=terrain[at][forward];
        if(next<0 || portalAt[at][forward]>=0) break;
        int j=local(decodePos(next));
        if(j>=0 && tiles[j].id>=0) break;
        meeting=decodePos(next);at=next;
    }
    double best=-1e20; int d=-1;
    for(const auto &m:moves) if(m.to>=0 && safeMove(m)) {
        // Prefer delivering in front of the queen's motion, rather than chasing its tail.
        double score=-distance(tiles[m.to].p,meeting)*12.0 + m.score*0.08;
        if((tiles[m.to].p.x==tiles[h].p.x || tiles[m.to].p.y==tiles[h].p.y) &&
           distance(tiles[m.to].p,tiles[h].p)<=3) score+=8;
        if(score>best) { best=score; d=m.dir; }
    }
    return d>=0 && promote(moves,d);
}

inline bool Bot::eligibleDonor(int donorId,int donorLength) {
    int cap=officialQueenDeath>=0 && roundNo>=400?24:12;
    if(donorId<=1 || donorLength<2 || donorLength>cap) return false;
    if(feedTargetId<=1) {
        // Fixed queen outranks a longer collector. Grow it progressively,
        // preserving most scouts until the collection phase.
        return roundNo>=policy::SupportStart && feedTargetLength>=2 &&
            donorLength<=(roundNo<330?4:roundNo<350?6:12);
    }
    if(officialQueenDeath<0 || roundNo<350 || feedTargetLength<4) return false;
    return feedTargetLength>donorLength ||
        (roundNo>=400 && donorLength==feedTargetLength && feedTargetId<donorId);
}

inline bool Bot::donationCandidate() {
    // Keep a collector beside the queen until the final collection window.
    if(quietEndgame() && feedTargetId<=1 && roundNo<470 && units<=2) return false;
    return !officialQueen() && feedTargetFresh() && feedTargetId!=id && eligibleDonor(id,length);
}

inline bool Bot::circleMeeting(vector<Move>& moves,int meeting) {
    if(meeting<0) return false;
    int d=-1;double best=-1e20;
    for(const Move &m:moves) if(m.to>=0 && safeMove(m)) {
        double score=m.score*0.12-distance(tiles[m.to].p,decodePos(meeting))*14.0;
        if(score>best) {best=score;d=m.dir;}
    }
    return d>=0 && promote(moves,d);
}

inline bool Bot::donorHasTurn() {
    if(servingUntil>=roundNo && servingDonor>=0) return servingDonor==id;
    int h=visibleHead(feedTargetId);if(h<0) return true;
    int best=id,near=distance(tiles[24].p,tiles[h].p);
    for(const Tile &t:tiles) if(t.head && t.team==team && t.id!=id && t.id!=feedTargetId) {
        auto a=allies.find(t.id);if(a==allies.end() || roundNo-a->second.stamp>4 || !eligibleDonor(t.id,a->second.len) ||
            (a->second.role!=1 && a->second.role!=6)) continue;
        int dist=distance(t.p,tiles[h].p);
        if(dist<=4 && (dist<near || (dist==near && t.id<best))) {best=t.id;near=dist;}
    }
    return best==id;
}

inline bool Bot::queenAssembly(vector<Move>& moves) {
    if((!officialQueen() && roundNo<300) || (officialQueen() && roundNo<8) || feedTargetId!=id || length<4 || receiveDonor>=0 || moves.empty()) return false;
    if(officialQueen() && spawnHome>=0 && homeDistance(tiles[24].p,spawnHome)<=max(4,length/4+2)) {
        bool enemy=false;
        for(const Tile &t:tiles) if(t.id>=0 && t.head && t.team!=team && distance(t.p,decodePos(spawnHome))<=5) enemy=true;
        if(!enemy && assemblyCenter<0) assemblyCenter=spawnHome;
    }
    if(assemblyCenter<0) {
        array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
        Region room=region(24,blocked,false);
        if(room.area>=min(length+8,28) && room.exits>=2) {
            assemblyCenter=code(tiles[24].p);role=5;return false;
        }
    }
    array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
    Region here=region(24,blocked,false);
    if(here.area>=min(length+8,28) && here.exits>=2) {role=5;return false;}
    auto best=moves.end();double value=-1e20;
    for(auto it=moves.begin();it!=moves.end();++it) if(it->to>=0 && safeMove(*it)) {
        double score=2.0*min(it->area,32)+5.0*min(it->exits,3)+it->score*0.2;
        if(assemblyCenter>=0) score-=4.0*max(0,homeDistance(tiles[it->to].p,assemblyCenter)-max(4,length/4+2));
        if(score>value) {value=score;best=it;}
    }
    if(best==moves.end()) return false;
    if(assemblyCenter<0 && best->area>=min(length+8,28) && best->exits>=2) assemblyCenter=code(tiles[best->to].p);
    rotate(moves.begin(),best,best+1);role=5;return true;
}

inline int Bot::giftExpiry() {
    int expiry=min(499,roundNo+64);
    if(receiveDonor>=0 && receiveStarted>=0) expiry=min(expiry,receiveStarted+64);
    return expiry;
}

// Sonar length selects the role; local recovery checks select the sacrifice turn.
inline bool Bot::coordinateDonation(vector<Move>& moves,Move &chosen,string &reason) {
    if(roundNo<(feedTargetId<=1 && feedTargetId>=0?policy::SupportStart:350) || roundNo>=497 || roundNo<=courierUntil ||
       roundNo<=reverseChildUntil) return false;
    if(!feedTargetFresh()) { donationTarget=-1;receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;return false; }
    if(receiveDonor>=0 && (roundNo>receiveUntil || feedTargetId!=id)) {
        receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;
    }
    // A donor STATUS is also an intent signal. Do not stall the receiver merely
    // because the narrow OFFER beam did not hit its body this turn.
    if(feedTargetId==id && receiveDonor<0) {
        int candidate=-1,near=1000;
        for(auto [owner,a]:allies) if(a.role==1 && freshAlly(owner) && eligibleDonor(owner,a.len)) {
            Collection plan=collectionPlan(owner,id,length);
            if(plan.safe && plan.dist<near) {candidate=owner;near=plan.dist;}
        }
        if(candidate>=0 && none_of(received.begin(),received.end(),[&](const Packet &p){return p.type==OFFER && p.sender==candidate;}))
            received.push_back({OFFER,candidate,allies[candidate].stamp,unsigned(id)});
    }
    for(const Packet &p:received) if(feedTargetId==id && p.type==OFFER && int(p.payload&65535)==id &&
        freshAlly(p.sender) &&
        eligibleDonor(p.sender,allies[p.sender].len) &&
        (receiveDonor<0 || receiveDonor==p.sender)) {
        Collection plan=collectionPlan(p.sender,id,length);
        if(plan.safe && any_of(moves.begin(),moves.end(),[this](const Move &m){return safeMove(m);})) {
            if(receiveDonor<0) {
                receiveDonor=p.sender;receiveStarted=roundNo;receiveUntil=giftExpiry();
                receiveConfirmed=false;
            }
            receiveMeeting=code(tiles[plan.closest].p);
            for(const Tile &t:tiles) if(t.id==p.sender) reservedFood[code(t.p)]={id,giftExpiry()};
            if(!receiveConfirmed) {
                receivePearls.clear();
                auto parts=visibleBody(p.sender,allies[p.sender].len);
                for(int k=0;k<int(parts.size());k+=2) receivePearls.push_back(code(tiles[parts[k]].p));
            }
            sendPacket(ACCEPT,unsigned(p.sender)|(unsigned(p.stamp)<<16),roundNo,true);
        }
    }
    if(receiveDonor>=0) {
        donationTarget=-1;role=2;
        // Refresh actual donor positions while it lives. An OFFER is not a pearl.
        if(freshAlly(receiveDonor) && visibleHead(receiveDonor)>=0) {
            auto parts=visibleBody(receiveDonor,allies[receiveDonor].len);
            receivePearls.clear();
            for(int k=0;k<int(parts.size());k+=2) receivePearls.push_back(code(tiles[parts[k]].p));
        }
        int best=-1,bestDist=10000;vector<int> actual,pending;
        for(int key:receivePearls) {
            int j=local(decodePos(key));
            if(j<0) {pending.push_back(key);continue;}
            if(!tiles[j].pearl) continue;
            actual.push_back(key);pending.push_back(key);
            int d=routeDirection(moves,key),dist=distance(tiles[24].p,decodePos(key));
            if(d>=0 && dist<bestDist) {bestDist=dist;best=d;}
        }
        if(!actual.empty()) receiveConfirmed=true;
        if(receiveConfirmed) {
            receivePearls=pending;
            if(best>=0 && promote(moves,best)) {
                reason="RECEIVE_PEARLS";receiveUntil=giftExpiry();return false;
            }
            for(int key:receivePearls) {
                int d=routeDirection(moves,key);
                if(d>=0 && promote(moves,d)) {reason="RECEIVE_PEARLS";return false;}
            }
            if(!receivePearls.empty() && roundNo<=receiveUntil) {
                reason="RECEIVE_REPOSITION";return false;
            }
            receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;
        } else if(roundNo<=receiveUntil && circleMeeting(moves,receiveMeeting)) {
            reason="RECEIVE_WAIT";return false;
        } else {
            receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;
        }
        role=0;
    }
    if(!donationCandidate() || roundNo<donorCooldown) {donationTarget=-1;return false;}
    int target=feedTargetId;
    if(!donorHasTurn()) {
        int best=-1;double score=-1e20;
        for(const Move &m:moves) if(m.to>=0 && safeMove(m)) {
            int dist=distance(tiles[m.to].p,feedTargetHead);
            double value=m.score+min(dist,5)*12.0;
            if(value>score) {score=value;best=m.dir;}
        }
        if(best>=0 && promote(moves,best)) {role=6;reason="DONOR_WAIT_CLEAR";}
        return false;
    }
    Pos meeting=feedTargetHead;
    auto beacon=rallyReports.find(target);
    if(beacon!=rallyReports.end() && roundNo-beacon->second.second<=20) meeting=decodePos(beacon->second.first);
    int travel=distance(tiles[24].p,meeting);
    if(donorTravelOwner!=target) {donorTravelOwner=target;donorBestDistance=travel;donorProgress=roundNo;}
    if(travel<donorBestDistance) {donorBestDistance=travel;donorProgress=roundNo;}
    if(travel>3 && roundNo-donorProgress>=32) {
        donationTarget=-1;donorTravelOwner=-1;donorCooldown=roundNo+4;return false;
    }
    if(roundNo+travel+(length+1)/2*2+6>=500) {donationTarget=-1;return false;}
    bool nearby=freshAlly(target) && visibleHead(target)>=0;
    Collection plan;
    if(nearby && (allies[target].role==0 || allies[target].role==2 || allies[target].role==5))
        plan=collectionPlan(id,target,allies[target].len);
    if(nearby) travel=distance(tiles[24].p,feedTargetHead);
    if(!nearby || !plan.safe) {
        donationTarget=-1;
        int d=nearby?-1:routeToMeeting(moves,code(meeting));
        if(nearby && approachAlly(moves,target)) {role=1;reason="DONOR_SEEK";}
        else {
            if(d<0) {
                int closest=travel;
                for(const Move &m:moves) if(m.to>=0 && safeMove(m)) {
                    int next=distance(tiles[m.to].p,meeting);
                    if(next<closest) {closest=next;d=m.dir;}
                }
            }
            if(d>=0 && promote(moves,d)) {role=1;reason="DONOR_SEEK";}
        }
        return false;
    }
    // With a live queen two steps away and every dropped pearl reachable, a
    // cheap donor can act immediately. Sonar length is sufficient role consent.
    if((target<=1 || roundNo>=400) && length<=6 && eligibleDonor(id,length) &&
       roundNo-allies[target].stamp<=2 && travel<=2 && plan.dist<=2 &&
       plan.count>=(length+1)/2) {
        int d=deliberateDeathDirection();
        if(d>=0) {donationTarget=target;chosen.dir=d;role=1;reason="DONATE_DIRECT";return true;}
    }
    if(donationTarget!=target) {
        donationTarget=target;donationTargetSince=roundNo;offerStamp=roundNo;
    } else if(roundNo-offerStamp>4) offerStamp=roundNo;
    role=1;
    for(const Packet &p:received) if(p.type==ACCEPT && p.sender==target &&
        int(p.payload&65535)==id && int((p.payload>>16)&511)==offerStamp && roundNo-p.stamp<=3) {
        int d=deliberateDeathDirection();
        if(d>=0) {chosen.dir=d;reason="DONATE_CONFIRMED";return true;}
    }
    sendPacket(OFFER,unsigned(target),offerStamp,true);
    // A short gift may proceed after repeated missed replies, with a live visible queen.
    if(((target<=1 && roundNo>=330) || roundNo>=350) && length<=12 && allies[target].len>=4 &&
       roundNo-donationTargetSince>=3 && travel<=3) {
        int d=deliberateDeathDirection();
        if(d>=0) {chosen.dir=d;reason="DONATE_NEAR";return true;}
    }
    if(circleMeeting(moves,code(tiles[plan.closest].p))) reason="DONOR_APPROACH";
    return false;
}

inline int Bot::donorSprint(const Move &first,const string &reason) {
    if(roundNo<350 || length<5 || officialQueen() || feedTargetId==id || role!=1 ||
       (reason!="DONOR_SEEK" && reason!="DONOR_APPROACH") || !feedTargetFresh() ||
       roundNo-feedTargetStamp>8 || first.to<0 || first.portal || !safeMove(first) || !donorHasTurn()) return -1;
    if(any_of(body.begin(),body.end(),[](Pos p){return p.x<0;})) return -1;
    for(const Tile &t:tiles) if(t.id>=0 && t.team!=team) return -1;
    Pos target=feedTargetHead;
    auto beacon=rallyReports.find(feedTargetId);
    if(beacon!=rallyReports.end() && roundNo-beacon->second.second<=8) target=decodePos(beacon->second.first);
    int distanceNow=distance(tiles[24].p,target),distanceFirst=distance(tiles[first.to].p,target);
    if(distanceNow<5 || distanceFirst>=distanceNow) return -1;
    for(const Tile &t:tiles) if(t.id>=0 && t.id!=id && t.team==team && distance(t.p,tiles[first.to].p)<=2) return -1;
    int best=-1;double value=-1e20;
    for(int d=0;d<4;d++) {
        if(edge(first.to,d)!=".") continue;
        int to=nextTile(first.to,d,false);
        if(to<0 || tiles[to].id>=0 || headRisk(to)>=18 || avoided(code(tiles[first.to].p),d)) continue;
        int nextDistance=distance(tiles[to].p,target);
        if(nextDistance<3 || nextDistance>=distanceFirst || obstructsQueen(to)) continue;
        bool crowded=false;
        for(const Tile &t:tiles) if(t.id>=0 && t.id!=id && t.team==team && distance(t.p,tiles[to].p)<=2) crowded=true;
        if(crowded || !pearlPathSafe({code(tiles[first.to].p),code(tiles[to].p)})) continue;
        double score=-nextDistance*10+int(d==first.dir)*2-headRisk(to);
        if(score>value) {value=score;best=d;}
    }
    return best;
}

} // namespace battlecode
