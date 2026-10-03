#pragma once
#include "bot.hpp"

namespace battlecode {

inline bool Bot::initialize() {
    string tag;
    if(!(cin>>tag>>id) || tag!="ID") return false;
    if(!(cin>>tag>>team>>tag>>width>>height>>tag>>unitLimit)) return false;
    lastVisit.fill(-10000); lastSeen.fill(-1); localAt.fill(-1); pearlSeen.fill(-1);
    spawnOneRound.fill(-100);harvestRound.fill(-100);
    for(auto &row:terrain) row.fill(-2);
    for(auto &row:portalAt) row.fill(-1);
    return true;
}

// Terminal no-action turns intentionally skip memory updates and age increments.
inline bool Bot::handleTerminalAction(bool &recoveredQueen) {
    hasOutgoing=false; role=0;attackSplitRadio=false;
    if(discardTrappedHead && retainedQueenCanEscape()) {
        discardTrappedHead=false;recoveredQueen=true;
    }
    if(discardTrappedHead) {
        // No action kills this two-segment parent in place, without hitting
        // another dragon. The reversed child is controlled independently.
        cout<<"INDICATOR "<<BotVersion<<" DISCARD_TRAPPED_HEAD_2\nPROTOCOL 3\nENDTURN\n"<<flush;
        return true;
    }
    if(newbornQueenTailGift()) {
        cout<<"INDICATOR "<<BotVersion<<" QUEEN_TAIL_GIFT_2\nPROTOCOL 3\nENDTURN\n"<<flush;
        return true;
    }
    return false;
}

inline void Bot::updateTurnMemory() {
    if(officialQueen() && spawnHome<0) {spawnHome=code(tiles[24].p);assemblyCenter=spawnHome;}
    observePortalArrival();
    if(turnsAlive==0 && length==2 && courierEntry<0 && terrainDegree(code(tiles[24].p))<=2) courierUntil=roundNo+16;
    int headKey=code(tiles[24].p); visits[headKey]++; lastVisit[headKey]=roundNo;
    if(assemblyCenter>=0 && protectedLong()) {
        bool threatened=false;
        for(const Tile &t:tiles) if(t.head && t.team!=team && t.id>=0 &&
            distance(t.p,decodePos(assemblyCenter))<=5) threatened=true;
        array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
        Region room=region(24,blocked,false);
        if(threatened || (!room.frontier && room.area<length+4) || room.exits==0) assemblyCenter=-1;
    }
}

// The order of guidance and coordination is part of the strategy contract.
inline ActionPlan Bot::planAction() {
    ActionPlan plan;
    auto &moves=plan.moves;
    auto &chosen=plan.chosen;
    auto &reason=plan.reason;
    auto &courier=plan.courier;
    moves=legalMoves();
    bool guided=guideOutOfLoop(moves);
    courier=guideCourier(moves);
    bool escapeLocked=guided && turnsAlive<escapeLockUntil;
    bool foodRoute=!courier && !escapeLocked && guideToPearl(moves);
    bool resourceRoute=!courier && !escapeLocked && !foodRoute && guideToResource(moves);
    bool pocket=!courier && !escapeLocked && profitablePocketMove(moves);
    reason=courier?"COURIER_ESCAPE":pocket?"PROFITABLE_POCKET":foodRoute?"PEARL_ROUTE":resourceRoute?"RESOURCE_ROUTE":guided?"NAVIGATE":"EXPLORE";
    if(!courier && queenAssembly(moves)) reason="QUEEN_ASSEMBLY";
    bool donated=coordinateDonation(moves,chosen,reason);
    if(!donated && chosen.dir<0) {
        int trade=favorableHeadTrade();
        if(trade>=0) { chosen.dir=trade; reason=newbornAmbush()?"TAIL_AMBUSH_TRADE":
            roundNo>=350?"QUEEN_GUARD_TRADE":"FAVORABLE_HEAD_TRADE"; }
    }
    if(!donated && chosen.dir<0 && coordinateHunt(moves)) reason="HUNT_BLOCK";
    // A two-segment child releases its tail for a trapped parent. If eating would
    // retain the blocking tail, take a non-food move; otherwise sacrifice ONLY the child.
    if(!donated && !officialQueen() && trappedAllyAtTail()) {
        auto it=find_if(moves.begin(),moves.end(),[this](const Move&m){return m.to>=0 && !tiles[m.to].pearl;});
        if(it!=moves.end()) { chosen=*it; reason="RELEASE_TAIL"; }
        else {
            int tail=local(body.back());
            for(int d=0;d<4;d++) if(nextTile(24,d,false)==tail) { chosen.dir=d; break; }
            reason="SACRIFICE_TWO";
        }
    }
    if(!donated) relieveQueenLane(plan);
    return plan;
}

// Preserve v13's exact split/trade/portal/failure precedence.
inline void Bot::chooseFallback(ActionPlan &plan) {
    auto &moves=plan.moves;
    auto &chosen=plan.chosen;
    auto &reason=plan.reason;
    auto &splitSize=plan.splitSize;
    bool courier=plan.courier;
    if(plan.sacrifice) return;
    if(chosen.dir<0) {
        bool canSplit=length>=4 && units<unitLimit;
        int queenCut=canSplit?queenReleaseSplitSize():0;
        if(queenCut>0) {
            splitSize=queenCut;reason="QUEEN_BODY_RELEASE";
        } else if(canSplit && reverseRescue(moves)) {
            splitSize=length-2; reason="REVERSE_RESCUE";
        } else if(canSplit && pocketEscapeSplit(moves)) {
            splitSize=length-2;reason="POCKET_ESCAPE_TWO_RETAINED";
        } else if(moves.empty() && emergencyPortalDirection()>=0) {
            chosen.dir=emergencyPortalDirection(); chosen.portal=true;
            chosen.portalId=stoi(edge(24,chosen.dir)); reason="LAST_RESORT_PORTAL";
        } else if(canSplit && !officialQueen() && ((moves.empty() && !protectedLong()) ||
            ((moves.empty() || moves.front().exits==0) && cutCanReleaseParent()))) {
            splitSize=2; reason=cutCanReleaseParent()?"RESCUE_SPLIT_2":"LAST_RESORT_SPLIT_2";
        } else if(int enemy=tailAmbushEnemy(moves);!courier && enemy>=0) {
            splitSize=2;reason="TAIL_AMBUSH_SPLIT";
            if(enemy<=4095) {
                sendPacket(HUNT,unsigned(code(body.back()))|(unsigned(enemy)<<12));
                attackSplitRadio=true;
            }
        } else if(int fork=branchChildDirection(moves);!courier && fork>=0) {
            splitSize=2;reason="BRANCH_SPLIT";
            sendPacket(ESCAPE,unsigned(code(body.back()))|(4094u<<12)|(unsigned(fork)<<24),roundNo,true);
        } else if(!courier && role==0 && routineSplit(moves)) {
            splitSize=2; reason="SCOUT_SPLIT_2";
        } else if(!moves.empty()) {
            auto safe=find_if(moves.begin(),moves.end(),[this](const Move &m){return safeMove(m);});
            chosen=officialQueen() && safe!=moves.end()?*safe:moves.front();
            if(officialQueen() && !chosen.sustainable) reason="QUEEN_EMERGENCY_STEP";
            if(chosen.uncertain && reason=="EXPLORE")
                reason=portalDestination(code(tiles[24].p),chosen.dir,chosen.portalId)>=0?
                    "PORTAL_ROUTE":"PORTAL_SCOUT";
        }
        else {
            // Unknown exits are used only when every verified move is impossible.
            for(int d=0;d<4;d++) if(!protectedLong() && edge(24,d)!="." &&
                edge(24,d)!="w") {
                chosen.dir=d; chosen.portal=true; chosen.portalId=stoi(edge(24,d));
                reason="LAST_RESORT_PORTAL"; break;
            }
            if(chosen.dir<0) {
                // Never take an ally's head down with us when escape is impossible.
                int bestLoss=100;
                for(int d=0;d<4;d++) {
                    int to=nextTile(24,d,false),loss=100;
                    if(to>=0 && tiles[to].id==id) loss=0;
                    else if(edge(24,d)=="w") loss=1;
                    else if(to>=0 && tiles[to].id>=0 && !tiles[to].head) loss=2;
                    else if(to>=0 && tiles[to].head && tiles[to].team!=team) loss=3;
                    if(loss<bestLoss) { bestLoss=loss; chosen.dir=d; }
                }
                if(chosen.dir<0) chosen.dir=(facing+2)%4;
                reason="NO_LEGAL_ESCAPE";
            }
        }
    }
}

inline void Bot::executeAction(ActionPlan &plan) {
    auto &chosen=plan.chosen;
    auto &reason=plan.reason;
    int splitSize=plan.splitSize;
    if(reason.rfind("DONATE_",0)==0 && donationTarget>=0)
        sendPacket(OFFER,unsigned(donationTarget),roundNo,true);
    if(plan.sacrifice) return; // Deliberate no-action death opens the lane without hitting an ally.
    if(splitSize) {
        if(protectedLong() && previousEntry>=0 && previousDirection>=0)
            queenBadEdges[previousEntry*4+previousDirection]=roundNo+80;
        if(reason=="QUEEN_BODY_RELEASE") {
            sendPacket(ESCAPE,unsigned(code(body.back()))|(4093u<<12),roundNo,true);
        } else if(reason=="REVERSE_RESCUE" || reason=="POCKET_ESCAPE_TWO_RETAINED") {
            discardTrappedHead=true;
            unsigned data=unsigned(code(body.back()))|(4095u<<12);
            sendPacket(ESCAPE,data,roundNo,true);
        } else if(reason!="SCOUT_SPLIT_2" && reason!="TAIL_AMBUSH_SPLIT" && reason!="BRANCH_SPLIT") prepareEscapeRadio();
        cout<<"SPLIT "<<splitSize<<'\n';
        body.resize(length-splitSize); lastSplit=roundNo;
    } else {
        int second=donorSprint(chosen,reason);
        cout<<"MOVE "<<DIRS[chosen.dir];
        if(second>=0) cout<<DIRS[second];
        cout<<'\n';
        int secondTo=second<0?-1:nextTile(chosen.to,second,false);
        if(chosen.to>=0 || chosen.portal) recordMove(chosen);
        if(second>=0) {Move next;next.dir=second;next.to=secondTo;recordMove(next);reason="DONOR_SPRINT";}
    }
}

inline void Bot::finishTurn(const ActionPlan &plan) {
    const auto &reason=plan.reason;
    emitRadio();
    cout<<"INDICATOR "<<BotVersion<<" "<<reason<<" bearing="<<DIRS[bearing]<<" goal="<<goal<<" peer="<<donationTarget<<" target="<<feedTargetId<<" queen="<<officialQueenId<<" enemy="<<huntEnemy<<" entry="<<corridorEntry<<" resource="<<resourceGoal<<" official="<<officialQueenId<<" queen_dead="<<officialQueenDeath<<" home="<<assemblyCenter<<" spawn="<<spawnHome<<" quiet="<<quietEndgame()<<"\nPROTOCOL 3\nENDTURN\n"<<flush;
}

inline void Bot::playTurn() {
    bool recoveredQueen=false;
    if(handleTerminalAction(recoveredQueen)) return;
    updateTurnMemory();
    ActionPlan plan=planAction();
    if(plan.chosen.dir<0) chooseFallback(plan);
    executeAction(plan);
    if(recoveredQueen && !plan.splitSize) plan.reason="QUEEN_RECOVERED";
    finishTurn(plan);
    ++turnsAlive;
}

inline int Bot::run() {
    // Same official-helper buffering as before the structural refactor.
    static char outputBuffer[8192];
    setvbuf(stdout,outputBuffer,_IOFBF,sizeof(outputBuffer));
    cin.tie(nullptr);
    if(!initialize()) return 0;
    while(readTurn()) playTurn();
    return 0;
}

} // namespace battlecode
