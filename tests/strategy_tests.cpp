#include "../src/controller.hpp"
#include <cassert>

namespace battlecode {

// A narrow friend fixture, not a production public-state compatibility layer.
struct BotTestAccess : Bot {

void setup(int len=3,int r=100) {
    pearlCooldown.fill(0);escapeLockUntil=-1;lastEnemySeen=-100;
    width=height=20; id=10; team='A'; length=len; roundNo=r; units=2; unitLimit=64; facing=bearing=1;
    localAt.fill(-1); lastSeen.fill(-1); lastVisit.fill(-10000); pearlSeen.fill(-1);
    for(auto &v:terrain) v.fill(-2);
    for(auto &v:portalAt) v.fill(-1);
    portalPairs.clear(); knownPortalEdges.clear(); portalLastUsed.clear();
    sharedPortalExit.clear();portalRouteLastSent.clear();warnings.clear();allies.clear();received.clear();
    warningSequence=0;
    attackChildEnemy=-1;attackChildUntil=-1;attackSplitRadio=false;
    assemblyCenter=-1;spawnHome=-1;feedTargetSince=-1;rallyReports.clear();donorTravelOwner=-1;donorBestDistance=1000;donorProgress=-1;donorCooldown=-1;
    // Legacy longest-mode fixtures start after a certified queen death.
    officialQueenId=-1;officialQueenDeath=0;queenObservedRound=-1;queenGiftStamp=-100;servingDonor=-1;servingUntil=-1;previousEntry=-1;previousDirection=-1;queenBadEdges.clear();
    allyReports.clear();resources.clear();reservedFood.clear();retiredUntil=-1;
    receiveConfirmed=false;receiveStarted=-1;receiveMeeting=-1;receivePearls.clear();
    spawnOneRound.fill(-100);harvestRound.fill(-100);harvestCount.fill(0);
    huntHeads.clear();huntEnemy=-1;huntCooldown=-1;foodRounds.clear();
    feedTargetId=-1;feedTargetLength=0;feedTargetStamp=-100;queenLastSent=-100;hasOutgoing=false;
    corridorEntry=-1;courierUntil=-1;reverseChildUntil=-1;donationTarget=-1;receiveDonor=-1;
    for(int j=0;j<49;j++) { tiles[j]=Tile{}; tiles[j].p={7+j%7,7+j/7}; localAt[code(tiles[j].p)]=j; }
    for(auto &r:he) for(auto &e:r) e=".";
    for(auto &r:ve) for(auto &e:r) e=".";
    body.clear();
    for(int i=0;i<len;i++) { Pos p{10-i,10}; body.push_back(p); int j=local(p); if(j>=0) { tiles[j].id=id; tiles[j].team=team; tiles[j].head=i==0; tiles[j].dir=1; } }
    rememberTerrain();
}
void ally(int owner,vector<Pos> b,int stamp=400) {
    for(int k=0;k<int(b.size());k++) { int j=local(b[k]); assert(j>=0); tiles[j].id=owner;tiles[j].team=team;tiles[j].head=k==0;
        int d=0; if(k) for(int e=0;e<4;e++) if(b[k].x+DC[e]==b[k-1].x && b[k].y+DR[e]==b[k-1].y) d=e;
        tiles[j].dir=d;
    }
    allies[owner]={int(b.size()),stamp,0,b[0]};
    rememberAllyReport(owner,int(b.size()),b[0],stamp);
}
unsigned long long from(int owner,int type,unsigned data,int stamp) {
    int saved=id;id=owner;auto raw=encodePacket({type,owner,stamp,data});id=saved;return raw;
}
int execute(int argc,char**argv) {
    assert(argc==2); string test=argv[1];
    if(test=="state_isolation") {
        BotTestAccess first,second;
        first.setup(4,420);second.setup(3,100);
        first.visits[640]=9;first.huntEnemy=77;first.body.clear();
        first.warnings[40]={420,440,-100,false,1};
        first.sendPacket(DANGER,123);
        assert(second.length==3 && second.roundNo==100 && second.body.size()==3);
        assert(second.visits[640]==0 && second.huntEnemy==-1);
        assert(second.warnings.empty() && !second.hasOutgoing);
        cout<<"per-dragon observation, navigation, combat and radio memory are isolated passed\n";
    } else if(test=="v15_queen_middle") {
        setup(11,400);id=0;
        for(auto &t:tiles) t.id=-1;
        body={{10,10},{10,9},{11,9},{11,10},{11,11},{10,11},{9,11},{9,10},{9,9},{8,9},{8,10}};
        for(int k=0;k<11;k++) {int j=local(body[k]);tiles[j].id=0;tiles[j].team=team;tiles[j].head=k==0;}
        assert(!queenTailReleaseSplit());assert(!reverseRescue({}));
        assert(queenReleaseSplitSize()==4);
        ActionPlan p;chooseFallback(p);assert(p.splitSize==4 && p.reason=="QUEEN_BODY_RELEASE");
        units=unitLimit;assert(queenReleaseSplitSize()==0);
        cout<<"queen middle-body exit releases minimum suffix and retains fixed head passed\n";
    } else if(test=="v15_queen_signal") {
        setup(4,200);turnsAlive=0;
        inbox={from(0,ESCAPE,unsigned(code(tiles[24].p))|(4093u<<12),200)};
        receiveRadio();assert(newbornQueenTailGift());
        setup(4,200);turnsAlive=0;
        inbox={from(0,ESCAPE,unsigned(code({11,10}))|(4093u<<12),200)};
        receiveRadio();assert(!newbornQueenTailGift());
        setup(4,200);turnsAlive=0;
        inbox={from(0,ESCAPE,unsigned(code(tiles[24].p))|(4093u<<12),198)};
        receiveRadio();assert(!newbornQueenTailGift());
        cout<<"queen suffix sacrifice requires newborn, matching position and fresh request passed\n";
    } else if(test=="v15_death_evidence") {
        setup(3,100);officialQueenDeath=-1;
        int q=local({11,10});tiles[q].id=0;tiles[q].team=team;tiles[q].head=true;
        inbox.clear();receiveRadio();assert(queenObservedRound==100);
        tiles[q].id=-1;tiles[q].head=false;roundNo=101;
        receiveRadio();assert(officialQueenDeath==101);
        setup(3,100);officialQueenDeath=-1;officialQueenId=0;queenObservedRound=99;queenObservedHead={13,10};
        inbox.clear();receiveRadio();assert(officialQueenDeath<0); // A queen outside our full reachable region may live.
        setup(3,450);officialQueenDeath=-1;
        inbox={from(0,QUEEN,unsigned(code({11,10})),101)};
        receiveRadio();assert(officialQueenDeath==101 && officialQueenId==0);
        rememberAllyReport(77,8,{12,12},450);assert(feedTargetId==77 && eligibleDonor(id,7));
        roundNo=451;inbox.clear();receiveRadio();assert(officialQueenDeath==101);
        cout<<"visual death proof is conservative; old certificates persist for new descendants passed\n";
    } else if(test=="v15_lane_clear") {
        setup(3,100);officialQueenDeath=-1;ally(0,{{11,10},{12,10}},100);
        he[3][4]="w";he[4][4]="w";rememberTerrain();
        ActionPlan p;p.moves=legalMoves();assert(relieveQueenLane(p));
        assert(p.sacrifice && p.reason=="QUEEN_CLEAR_LANE" && p.chosen.dir<0);
        he[3][4]=".";rememberTerrain();p=ActionPlan{};p.moves=legalMoves();assert(!relieveQueenLane(p));
        cout<<"short queen lane is cleared before endgame only when no usable exit exists passed\n";
    } else if(test=="v15_pending_gift") {
        setup(8,400);rememberAllyReport(id,length,tiles[24].p,400);
        receiveDonor=20;receiveUntil=430;receiveConfirmed=true;receiveStarted=400;
        receivePearls={code({14,10})};auto moves=legalMoves();Move chosen;string reason;
        coordinateDonation(moves,chosen,reason);
        assert(receiveDonor==20 && receivePearls.size()==1);
        roundNo=431;rememberAllyReport(id,length,tiles[24].p,431);
        coordinateDonation(moves,chosen,reason);assert(receiveDonor<0);
        cout<<"confirmed out-of-view corpse pearls survive temporary detour and expire boundedly passed\n";
    } else if(test=="body_memory") {
        setup(8);
        auto old=body;syncBody();assert(body==old && local(body.back())<0);
        Move m;m.dir=1;m.to=local({11,10});recordMove(m);
        assert(body.size()==8 && body.back()==old[6]);
        // An unknown portal preserves the old rear, both with and without food.
        setup(6);old=body;m.to=-1;recordMove(m);
        for(auto &t:tiles) t.id=-1;tiles[24].p={2,2};syncBody();
        assert(body.size()==6 && body[1]==old[0] && body.back()==old[4]);
        setup(6);old=body;recordMove(m);length=7;for(auto &t:tiles) t.id=-1;tiles[24].p={2,2};syncBody();
        assert(body.size()==7 && body.back()==old.back());
        // Both four and six can reverse out; six has an offscreen tail.
        for(int len:{4,6}) {
            setup(len);id=10;
            ve[3][4]="w";he[3][3]="w";he[4][3]="w";
            rememberTerrain();
            for(int y=0;y<20;y++) for(int x=0;x<20;x++) for(int d=0;d<4;d++) {
                int k=code({x,y});
                if(terrain[k][d]==-2) terrain[k][d]=code({wrap(x+DC[d],20),wrap(y+DR[d],20)});
            }
            assert(reverseRescue({}) && length-(length-2)==2);
            Move safe;safe.sustainable=true;safe.exits=1;safe.risk=0;
            assert(!reverseRescue({safe}));
            units=unitLimit;assert(!reverseRescue({}));
        }
        cout<<"body memory, hidden tail and 4/6 reversal passed\n";
    } else if(test=="unsafe_pearl_path") {
        setup(3,100);turnsAlive=40;
        for(auto &t:tiles) if(t.id==id) t=Tile{t.p};
        body={{10,10},{10,11},{9,11}};
        for(int k=0;k<3;k++) {int j=local(body[k]);tiles[j].id=id;tiles[j].team=team;tiles[j].head=k==0;}
        for(auto &r:he) for(auto &e:r) e="w";
        for(auto &r:ve) for(auto &e:r) e="w";
        auto open=[this](Pos a,Pos b) {int j=local(a),r=j/7,c=j%7;
            if(b.x>a.x) ve[r][c+1]=".";else if(b.x<a.x) ve[r][c]=".";
            else if(b.y>a.y) he[r+1][c]=".";else he[r][c]=".";};
        vector<Pos> ring{{10,10},{9,10},{8,10},{8,11},{9,11},{10,11},{10,10}};
        for(int k=1;k<int(ring.size());k++) open(ring[k-1],ring[k]);
        open({8,11},{7,11});tiles[local({7,11})].pearl=true;
        vector<Pos> escape{{10,10},{11,10},{12,10},{12,9},{11,9},{10,9},{9,9},{8,9},{8,10}};
        for(int k=1;k<int(escape.size());k++) open(escape[k-1],escape[k]);
        rememberTerrain();auto m=legalMoves();
        assert(any_of(m.begin(),m.end(),[this](const Move&m){return m.dir==3 && safeMove(m);}));
        assert(!guideToPearl(m));assert(pearlCooldown[code({7,11})]==124);
        cout<<"safe first step cannot commit to a dead-end final meal; target cools down passed\n";
    } else if(test=="quiet_feeding") {
        setup(10,360);turnsAlive=80;rememberAllyReport(0,8,{13,12},roundNo);units=5;
        assert(quietEndgame() && donationCandidate());
        lastEnemySeen=350;assert(!quietEndgame() && donationCandidate()); // Local corpse safety, not distant past enemies, gates the gift.
        lastEnemySeen=-100;units=2;assert(!donationCandidate());
        roundNo=470;rememberAllyReport(0,8,{13,12},roundNo);assert(donationCandidate());
        cout<<"R350 queen feeding admits larger donors; recent enemies still require full local recovery passed\n";
    } else if(test=="short_loop") {
        setup();turnsAlive=40;visits[code(tiles[24].p)]=1;
        for(int j=0;j<24;j++) recentHeads.push_back(code({j%20,j/20}));
        vector<Pos> cycle{{10,9},{11,9},{11,10},{10,10}};
        for(int j=0;j<7;j++) recentHeads.push_back(code(cycle[j%4]));
        auto m=legalMoves();assert(guideOutOfLoop(m));assert(navigating && goal>=0 && escapeLockUntil==turnsAlive+8);
        for(Pos p:cycle) assert(goal!=code(p));
        assert(safeMove(m.front()));
        cout<<"two laps of a four-cell loop choose a reachable destination outside the loop passed\n";
    } else if(test=="frontier_scout") {
        setup();id=12;for(auto &t:tiles) if(t.id==10) t.id=id;turnsAlive=10;visits.fill(0);visits[code(tiles[24].p)]=1;
        for(const Tile &t:tiles) lastSeen[code(t.p)]=roundNo;
        auto moves=legalMoves();assert(guideOutOfLoop(moves));
        assert(navigating && goal>=0 && safeMove(moves.front()));
        cout<<"scout seeks an observation frontier before revisiting any head position passed\n";
    } else if(test=="official_queen") {
        setup(5,100);assert(routineSplit(legalMoves()));id=1;
        for(auto &t:tiles) if(t.id==10) t.id=id;
        rememberAllyReport(id,length,tiles[24].p,roundNo);
        rememberAllyReport(77,20,{12,12},roundNo);
        assert(feedTargetId==1 && feedTargetLength==5 && officialQueenId==1);
        assert(protectedLong() && !routineSplit(legalMoves()) && !donationCandidate());
        assert(favorableHeadTrade()<0);
        auto queenMoves=legalMoves();turnsAlive=40;assert(!guideOutOfLoop(queenMoves));
        ve[3][4]="7";rememberTerrain();
        for(const auto &m:legalMoves()) assert(!m.portal);
        setup(4,300);rememberAllyReport(0,3,{13,12},roundNo);
        assert(feedTargetId==0 && feedTargetLength==3 && donationCandidate());
        assert(eligibleDonor(10,4) && !eligibleDonor(1,2));
        roundNo=341;rememberAllyReport(id,length,tiles[24].p,roundNo);
        assert(!feedTargetFresh() && feedTargetId==-1); // Silence is not proof of queen death.
        rememberAllyReport(0,2,{13,12},roundNo);
        assert(feedTargetId==0 && feedTargetLength==2);
        cout<<"fixed queen outranks larger collectors, never scouts portals or donates, and survives shrinkage passed\n";
    } else if(test=="queen_lane") {
        setup(2,420);ally(77,{{12,10},{12,11},{12,12},{13,12},{13,11},{13,10},{13,9},{13,8}},420);
        assert(obstructsQueen(local({12,9})));
        Move lane;lane.dir=0;lane.to=local({12,9});lane.area=20;lane.exits=2;
        assert(!safeMove(lane));lane.to=local({11,9});assert(safeMove(lane));
        cout<<"all route promotions respect the queen's next head tile passed\n";
    } else if(test=="queen_hold") {
        setup(8,420);rememberAllyReport(id,length,tiles[24].p,420);receiveDonor=20;receiveUntil=426;
        rememberAllyReport(77,9,{12,10},420);assert(feedTargetId==id);
        rememberAllyReport(88,12,{11,12},420);assert(feedTargetId==88);
        receiveDonor=-1;roundNo=421;rememberAllyReport(88,2,{11,12},421);assert(feedTargetId==77);
        cout<<"receiver finishes an active gift, while larger challengers and shrinkage still replace it passed\n";
    } else if(test=="queen_handoff") {
        setup(8,420);turnsAlive=0;
        inbox={from(2,ESCAPE,unsigned(code(tiles[24].p))|(4095u<<12),420)};
        rememberAllyReport(0,2,{13,13},420);
        receiveRadio();assert(feedTargetId==0 && reverseChildUntil==422);
        assert(courierEntry<0 && warnings.empty());
        cout<<"reversed child preserves fixed queen identity without a phantom warning passed\n";
    } else if(test=="donor_queue") {
        setup(2,420);ally(77,{{12,10},{12,11},{12,12},{13,12},{13,11},{13,10},{13,9},{13,8}},420);
        ally(20,{{11,10},{11,11}},420);assert(donorHasTurn());
        allies[20].role=1;assert(!donorHasTurn());
        allies[20].len=8;assert(donorHasTurn()); // Equal-length contenders do not donate to a higher ID.
        allies[20].len=2;
        servingDonor=id;servingUntil=426;assert(donorHasTurn());
        auto moves=legalMoves();Move chosen;string reason;
        servingDonor=20;assert(!coordinateDonation(moves,chosen,reason));
        assert(role==6 && reason=="DONOR_WAIT_CLEAR");
        cout<<"nearest/admitted donor approaches; others clear the queen lane passed\n";
    } else if(test=="hunt_selective") {
        setup();units=6;ally(2,{{10,8},{9,8}},100);
        int enemy=local({12,9});tiles[enemy].id=99;tiles[enemy].team='B';tiles[enemy].head=true;
        auto moves=legalMoves();assert(!coordinateHunt(moves));assert(role!=4);
        cout<<"scouts decline a tiny target with open escape routes passed\n";
    } else if(test=="warnings") {
        setup(); addWarning(code(tiles[24].p),1,roundNo,true,true);
        auto m=legalMoves();assert(m.front().dir!=1);
        assert(!avoided(code(tiles[24].p),3));
        addWarning(code({9,9}),0,roundNo,false);roundNo+=21;assert(!avoided(code({9,9}),0));
        cout<<"directional entrance warning and temporary expiry passed\n";
    } else if(test=="escape") {
        setup(2);turnsAlive=0;unsigned data=code(tiles[24].p)|(code({7,10})<<12)|(1u<<24)|(1u<<26);
        inbox={from(2,ESCAPE,data,roundNo)};receiveRadio();
        assert(courierEntry==code({7,10}) && courierUntil==132);
        assert(avoided(code({7,10}),1));
        auto moves=legalMoves();assert(guideCourier(moves));assert(role==3 && outgoing.type==DANGER);
        auto raw=from(2,STATUS,unsigned(code({11,10}))|(8u<<12),roundNo);inbox={raw};receiveRadio();assert(!allies.count(2));
        cout<<"newborn escape assignment, warning relay, nonvisible status rejection passed\n";
    } else if(test=="donation") {
        setup(2,360);ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8}},360);
        auto moves=legalMoves();Move chosen;string reason="EXPLORE";
        assert(collectionPlan(id,2,8).safe);
        assert(!coordinateDonation(moves,chosen,reason));assert(chosen.dir<0 && outgoing.type==OFFER);
        int stamp=offerStamp;
        received={{ACCEPT,2,360,unsigned(id)|(unsigned(stamp)<<16)}};
        assert(coordinateDonation(moves,chosen,reason));
        assert(reason=="DONATE_CONFIRMED" && chosen.dir==3); // own tail, not another head
        chosen=Move{};roundNo=349;assert(!coordinateDonation(moves,chosen,reason));
        roundNo=361;received[0].payload=11;assert(!coordinateDonation(moves,chosen,reason));
        cout<<"queen donation needs correct ACK, round >=350 and safe collection passed\n";
    } else if(test=="contested") {
        setup(2,400);ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8}},400);
        int j=local({11,11});tiles[j].id=99;tiles[j].head=true;tiles[j].team='B';
        assert(!collectionPlan(id,2,8).safe);
        auto moves=legalMoves();Move chosen;string reason;
        assert(!coordinateDonation(moves,chosen,reason));assert(donationTarget<0);
        cout<<"contested pearls block deliberate sacrifice passed\n";
    } else if(test=="receiver") {
        setup(8,400);rememberAllyReport(id,length,tiles[24].p,roundNo);
        ally(20,{{11,9},{12,9}},400);ally(21,{{12,11},{13,11}},400);
        received={{OFFER,20,400,unsigned(id)},{OFFER,21,400,unsigned(id)}};
        auto m=legalMoves();Move c;string reason;
        coordinateDonation(m,c,reason);
        assert(receiveDonor==20 && role==2 && outgoing.type==ACCEPT && int(outgoing.payload&65535)==20);
        assert(donationTarget<0);
        cout<<"receiver reserves one donor, never simultaneously donates passed\n";
    } else if(test=="late_donation") {
        setup(2,410);
        ally(2,{{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8},{9,9},{8,9},{8,8}},410);
        assert(collectionPlan(id,2,10).safe);
        auto m=legalMoves();Move c;string reason;
        for(int r=410;r<=413;r++) {
            roundNo=r;allies[2].stamp=r;hasOutgoing=false;c=Move{};
            bool done=coordinateDonation(m,c,reason);
            if(r<413) assert(!done && c.dir<0);
            else assert(done && reason=="DONATE_NEAR" && c.dir==3);
        }
        cout<<"nearby short donor consolidates after repeated unanswered offers passed\n";
     } else if(test=="hunt_join") {
        setup();units=4;ally(2,{{9,8},{8,8}},100);
        for(Pos p:vector<Pos>{{12,9},{12,8},{11,8},{11,7},{12,7},{13,7}}) {int j=local(p);tiles[j].id=99;tiles[j].team='B';tiles[j].head=p==Pos{12,9};}
        he[3][5]="w";rememberTerrain();
        received.clear(); // Visibility suffices; no received HUNT claim is needed.
        auto m=legalMoves();assert(coordinateHunt(m));assert(role==4 && outgoing.type==HUNT);
        assert(safeMove(m.front()));
        cout<<"two visible allies coordinate without a sonar handshake passed\n";
    } else if(test=="hunt_timeout") {
        setup();units=4;ally(2,{{9,8},{8,8}},100);
        int enemy=local({12,10});tiles[enemy].id=99;tiles[enemy].team='B';tiles[enemy].head=true;
        huntEnemy=99;huntSince=70;huntProgress=99;
        received={{HUNT,2,100,unsigned(code({12,10}))|(99u<<12)}};
        auto m=legalMoves();assert(!coordinateHunt(m));assert(huntEnemy<0 && huntCooldown==108);
        cout<<"hunting stops after thirty rounds passed\n";
    } else if(test=="split_radio") {
        setup(4);corridorEntry=code({8,10});corridorDir=1;
        ve[3][4]="w";he[3][3]="w";he[4][3]="w";rememberTerrain();
        prepareEscapeRadio();assert(outgoing.type==ESCAPE);
        assert((outgoing.payload&4095)==unsigned(code(body.back())));
        assert(((outgoing.payload>>12)&4095)==unsigned(corridorEntry));
        assert((outgoing.payload>>26)&1);
        cout<<"trapped parent sends tail-child destination and permanent wall warning passed\n";
    } else if(test=="reverse_rescue") {
        setup(3,410);
        for(auto &t:tiles) if(t.id==id) { t.id=-1; t.head=false; }
        body={{10,10},{10,11},{11,11},{12,11},{12,10},{12,9},
              {11,9},{10,9},{9,9},{9,10}};
        length=int(body.size());
        for(int n=0;n<length;n++) {
            int j=local(body[n]);assert(j>=0);
            tiles[j].id=id;tiles[j].team=team;tiles[j].head=n==0;
        }
        ve[3][4]="w"; // The only non-body direction from the old head is a wall.
        auto moves=legalMoves();
        // The tail is occupied during collision checking, with or without food.
        assert(moves.empty());
        assert(physicallyTrapped() && !cutCanReleaseParent() && reverseRescue(moves));
        tiles[local(body.back())].pearl=1;
        assert(physicallyTrapped() && reverseRescue(moves));
        tiles[local(body.back())].pearl=0;
        ve[3][4]="."; // An actually empty destination must still prevent discard.
        assert(!physicallyTrapped() && !reverseRescue(legalMoves()));
        ve[3][4]="w";
        units=unitLimit;assert(!reverseRescue({}));units=2;
        assert(length-2==8 && local(body.back())>=0);
        cout<<"trapped long parent leaves old head 2 and reverses 8 safe tail segments passed\n";
    } else if(test=="tail_collision_trap") {
        // M887299 R406 ID562, translated from (33,19) to (10,10).
        setup(3,406);turnsAlive=148;facing=bearing=3;
        for(auto &t:tiles) if(t.id==id) {t.id=-1;t.head=false;}
        body={{10,10},{11,10},{11,11},{10,11},{10,12},{9,12},{9,11},{9,10}};
        length=int(body.size());
        for(int n=0;n<length;n++) {
            int j=local(body[n]);assert(j>=0);
            tiles[j].id=id;tiles[j].team=team;tiles[j].head=n==0;
        }
        he[3][3]="w";he[4][3]="w";rememberTerrain();
        assert(legalMoves().empty());
        assert(physicallyTrapped() && reverseRescue({}));
        assert(!cutCanReleaseParent());
        units=unitLimit;assert(!reverseRescue({}));units=2;
        he[3][3]=".";rememberTerrain();
        assert(!physicallyTrapped() && !reverseRescue(legalMoves()));
        cout<<"replay R406 tail collision is trapped; a truly empty exit prevents rescue passed\n";
    } else if(test=="queen_tail_release") {
        // Schooltime's starting queen: head and tail are adjacent in a 2x2 coil.
        setup(3,0);id=0;turnsAlive=0;
        for(auto &t:tiles) if(t.id==10) {t.id=-1;t.head=false;}
        body={{10,10},{10,9},{11,9},{11,10}};length=4;
        for(int n=0;n<length;n++) {int j=local(body[n]);tiles[j].id=id;tiles[j].team=team;tiles[j].head=n==0;}
        he[4][3]="w";ve[3][3]="w";rememberTerrain();
        assert(queenTailReleaseSplit());
        units=unitLimit;assert(!queenTailReleaseSplit());units=2;
        ve[3][3]=".";assert(!queenTailReleaseSplit());ve[3][3]="w";
        id=2;assert(!queenTailReleaseSplit());
        // A visible queen with no ordinary exit may reclaim the newborn head.
        setup(2,0);turnsAlive=0;
        for(auto &t:tiles) if(t.id==id) {t.id=-1;t.head=false;}
        body={{10,10},{10,9}};
        for(int n=0;n<2;n++) {int j=local(body[n]);tiles[j].id=id;tiles[j].team=team;tiles[j].head=n==0;}
        ally(0,{{9,10},{9,9}},0);
        he[4][2]="w";ve[3][2]="w";rememberTerrain();
        assert(newbornQueenTailGift());
        turnsAlive=1;assert(!newbornQueenTailGift());turnsAlive=0;
        ve[3][2]=".";assert(!newbornQueenTailGift());
        ve[3][2]="5";assert(!newbornQueenTailGift());ve[3][2]="w";
        tiles[local({9,10})].team='B';assert(!newbornQueenTailGift());
        cout<<"queen keeps identity; newborn gift requires a visible trapped ally and no free/unknown exit passed\n";
    } else if(test=="queen_recover") {
        setup(2,220);id=0;
        for(auto &t:tiles) if(t.id==10) t.id=id;
        assert(retainedQueenCanEscape());
        he[3][3]="w";he[4][3]="w";ve[3][4]="w";rememberTerrain();
        assert(!retainedQueenCanEscape());
        id=2;assert(!retainedQueenCanEscape());
        cout<<"retained queen recovers only when a fresh safe move exists passed\n";
    } else if(test=="roles_and_income") {
        setup(4,100);turnsAlive=30;foodRounds.push_back(99);
        auto moves=legalMoves();assert(routineSplit(moves));
        roundNo=320;foodRounds.push_back(319);assert(!routineSplit(moves));
        roundNo=420;assert(!routineSplit(moves));
        foodRounds.push_back(419);assert(!routineSplit(moves));
        setup(8,420);assert(protectedLong());
        auto rich=legalMoves();assert(!routineSplit(rich));
        setup(5,320);foodRounds={300,319};
        auto productive=legalMoves();assert(!routineSplit(productive));
        foodRounds={319};assert(!routineSplit(productive));
        cout<<"early 2+2 scouts, income-gated late collectors and protected long dragon passed\n";
    } else if(test=="productive_loop") {
        setup(3,100);turnsAlive=40;visits[code(tiles[24].p)]=1;
        vector<Pos> cycle{{10,9},{11,9},{11,10},{10,10}};
        for(int j=0;j<16;j++) recentHeads.push_back(code(cycle[j%4]));
        foodRounds.push_back(99);
        auto m=legalMoves();assert(!guideOutOfLoop(m) && !navigating);
        cout<<"productive local loop is retained passed\n";
    } else if(test=="head_trade") {
        setup(2,100);units=5;
        for(Pos p:vector<Pos>{{11,10},{11,9},{12,9},{13,9},{13,8},{12,8}}) {
            int j=local(p);assert(j>=0);tiles[j].id=90;tiles[j].team='B';
            tiles[j].head=p==Pos{11,10};
        }
        assert(favorableHeadTrade()==1);
        units=4;assert(favorableHeadTrade()<0);
        units=5;roundNo=350;assert(favorableHeadTrade()<0);
        cout<<"only surplus short scouts may trade with a visibly longer enemy passed\n";
    } else if(test=="portal_route") {
        setup(2,100);turnsAlive=20;
        ve[3][4]="7";remember(7,false,{11,10});rememberTerrain();
        auto sparse=legalMoves();
        assert(find_if(sparse.begin(),sparse.end(),[](const Move&m){return m.portal;})==sparse.end());
        he[0][0]="8";remember(8,true,{7,7});rememberTerrain();
        auto first=legalMoves();
        auto unknown=find_if(first.begin(),first.end(),[](const Move&m){return m.portal;});
        assert(unknown!=first.end() && unknown->uncertain && unknown->to<0);
        assert(terrain[code({10,10})][1]==-3);
        remember(7,false,{2,2});rememberTerrain();
        assert(portalDestination(code({10,10}),1,7)==code({2,2}));
        assert(portalDestination(code({11,10}),3,7)==code({1,2}));
        assert(terrain[code({10,10})][1]==code({2,2}));
        auto known=legalMoves();
        auto route=find_if(known.begin(),known.end(),[](const Move&m){return m.portal;});
        assert(route!=known.end() && safeMove(*route));
        portalUses=5;recordMove(*route);
        assert(portalUses==6 && !portalReady(7) && body.front().x<0 && body.size()==size_t(length+1));
        auto cooling=legalMoves();
        assert(find_if(cooling.begin(),cooling.end(),[](const Move&m){return m.portal;})==cooling.end());
        assert(emergencyPortalDirection()==1);
        roundNo+=policy::PortalRetryDelay;assert(portalReady(7));
        roundNo=400;length=8;assert(protectedLong() && emergencyPortalDirection()<0);
        cout<<"unknown portal scouting, remote paired route, cooldown and no hard quota passed\n";
    } else if(test=="trap_share") {
        setup(2,100);turnsAlive=20;
        ve[3][4]="7";remember(7,false,{11,10});rememberTerrain();
        int entry=code({10,10}),exit=code({12,10}),key=entry*4+1;
        for(int d=0;d<4;d++) { terrain[exit][d]=-1;portalAt[exit][d]=-1; }
        terrain[exit][3]=-3;portalAt[exit][3]=7;
        inbox={from(2,PORTAL_ROUTE_PACKET,unsigned(entry)|(1u<<12)|(unsigned(exit)<<14),100),
               from(3,DANGER,unsigned(entry)|(1u<<12)|(1u<<14),100)};
        receiveRadio();
        assert(sharedPortalExit[key]==exit && knownPortalTrap(exit));
        assert(avoided(entry,1) && warnings[key].permanent);
        auto moves=legalMoves();
        assert(find_if(moves.begin(),moves.end(),[](const Move&m){return m.portal;})==moves.end());
        roundNo=400;assert(avoided(entry,1));
        cout<<"shared portal exits and permanent trap warnings prevent re-entry passed\n";
    } else if(test=="early_support") {
        setup(2,360);
        ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8}},360);
        auto moves=legalMoves();Move chosen;string reason;
        assert(collectionPlan(id,2,8).safe);
        assert(!coordinateDonation(moves,chosen,reason));
        assert(outgoing.type==OFFER && donationTarget==2);
        roundNo=349;donationTarget=-1;hasOutgoing=false;
        assert(!coordinateDonation(moves,chosen,reason));
        cout<<"small donors can approach length-eight receiver from round 350 passed\n";
    } else if(test=="queen_only") {
        setup(2,410);
        ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8}},410);
        auto moves=legalMoves();Move chosen;string reason;
        feedTargetId=99;feedTargetLength=12;feedTargetHead={15,12};feedTargetStamp=410;
        assert(!coordinateDonation(moves,chosen,reason));
        assert(donationTarget<0 && (!hasOutgoing || outgoing.type!=OFFER));
        int to=local({10,11});tiles[to].pearl=1;
        feedTargetHead={10,12};double reserved=evaluate(2,to).score;
        feedTargetId=id;double queenScore=evaluate(2,to).score;
        assert(reserved+90<queenScore);
        feedTargetId=2;feedTargetLength=8;feedTargetHead={11,9};feedTargetStamp=410;
        assert(coordinateDonation(moves,chosen,reason));
        assert(donationTarget==2 && reason=="DONATE_DIRECT" && chosen.dir==3);
        cout<<"safe direct gift needs a fresh elected queen, not an ACK passed\n";
    } else if(test=="warning_queue") {
        setup(2,100);
        addWarning(code({10,10}),0,100,true);
        for(int k=0;k<64;k++) addWarning(code({k%20,k/20}),1,100,false);
        assert(warnings.size()==64 && avoided(code({10,10}),0));
        assert(!avoided(code({0,0}),1) && avoided(code({1,0}),1));
        cout<<"warning queue evicts oldest temporary record before permanent danger passed\n";
    } else if(test=="queen_relay") {
        setup(2,420);turnsAlive=10;
        inbox={from(77,QUEEN,unsigned(code({13,12}))|(20u<<12),418)};
        receiveRadio();
        assert(feedTargetId==77 && feedTargetLength==20 && feedTargetHead==Pos(13,12));
        emitRadio();
        assert(outgoing.type==QUEEN && outgoing.sender==77 && outgoing.stamp==418);
        auto raw=encodePacket(outgoing);
        assert(int((raw>>36)&65535)==77 && int((raw>>27)&511)==418);
        cout<<"queen identity, length and position survive sonar relay passed\n";
    } else if(test=="wide_scout") {
        setup(2,100);width=48;height=24;turnsAlive=20;
        ve[3][4]="7";remember(7,false,{11,10});rememberTerrain();
        auto moves=legalMoves();
        assert(find_if(moves.begin(),moves.end(),[](const Move&m){return m.portal && m.uncertain && m.score>40;})!=moves.end());
        assert(!guideOutOfLoop(moves));
        setup(4,100);width=48;height=24;turnsAlive=20;units=4;
        moves=legalMoves();assert(routineSplit(moves));
        cout<<"large-map scouts take sparse portals and split without recent food passed\n";
    } else if(test=="pocket_profit") {
        setup(2,100);
        he[0][3]="w";
        for(int r=0;r<3;r++) ve[r][3]=ve[r][4]="w";
        for(int y=7;y<=9;y++) tiles[local({10,y})].pearl=1;
        rememberTerrain();auto moves=legalMoves();
        assert(profitablePocketMove(moves) && moves.front().dir==0);
        setup(5,100);
        for(auto &t:tiles) if(t.id==id) {t.id=-1;t.head=false;}
        body={{10,10},{10,11},{10,12},{11,12},{11,13}};
        for(int n=0;n<5;n++) {int j=local(body[n]);tiles[j].id=id;tiles[j].team=team;tiles[j].head=n==0;}
        he[3][3]="w";ve[3][3]=ve[3][4]="w";
        corridorEntry=code(body.back());moves=legalMoves();
        assert(pocketEscapeSplit(moves));
        cout<<"three-pearl pocket can be entered and a length-three tail can escape passed\n";
    } else if(test=="failed_gift") {
        setup(8,400);rememberAllyReport(id,length,tiles[24].p,roundNo);
        ally(20,{{11,9},{12,9}},400);
        received={{OFFER,20,400,unsigned(id)}};
        auto moves=legalMoves();Move chosen;string reason;
        coordinateDonation(moves,chosen,reason);
        assert(receiveDonor==20 && !receiveConfirmed && reason=="RECEIVE_WAIT");
        received.clear();
        int expiry=receiveUntil;
        for(int r=401;r<=expiry+1;r++) {
            roundNo=r;hasOutgoing=false;role=0;reason="EXPLORE";
            coordinateDonation(moves,chosen,reason);
            assert(reason!="RECEIVE_PEARLS");
        }
        assert(receiveDonor<0 && receivePearls.empty());
        cout<<"unfulfilled offer never becomes phantom food and expires passed\n";
    } else if(test=="real_gift") {
        setup(8,400);rememberAllyReport(id,length,tiles[24].p,roundNo);
        ally(20,{{11,9},{12,9}},400);
        received={{OFFER,20,400,unsigned(id)}};
        auto moves=legalMoves();Move chosen;string reason;
        coordinateDonation(moves,chosen,reason);
        for(auto &t:tiles) if(t.id==20) {t.id=-1;t.head=false;}
        tiles[local({11,9})].pearl=1;received.clear();roundNo++; // Stale living-donor STATUS must not erase the corpse.
        moves=legalMoves();coordinateDonation(moves,chosen,reason);
        assert(receiveConfirmed && reason=="RECEIVE_PEARLS");
        cout<<"receiver follows corpse pearls despite retained donor STATUS passed\n";
    } else if(test=="quiet_partial_gift") {
        setup(6,360);turnsAlive=80;
        ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8},{9,9},{8,9}},360);
        assert(collectionPlan(id,2,10).safe);
        lastEnemySeen=359;assert(!collectionPlan(id,2,10).safe);
        cout<<"quiet midgame accepts a safe visible portion; recent enemies require full recovery passed\n";
    } else if(test=="partial_gift") {
        setup(6,410);
        ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8},{9,9},{8,9}},410);
        assert(local(body.back())<0 && collectionPlan(id,2,10).safe);
        auto moves=legalMoves();Move chosen;string reason;
        assert(!coordinateDonation(moves,chosen,reason) && outgoing.type==OFFER);
        cout<<"safe partial gift does not need the whole donor inside vision passed\n";
    } else if(test=="queen_update") {
        setup(2,420);
        rememberAllyReport(77,20,{13,12},400);rememberAllyReport(88,12,{12,12},401);
        assert(feedTargetId==77);
        rememberAllyReport(77,2,{13,12},402);assert(feedTargetId==88);
        rememberAllyReport(77,20,{13,12},400);assert(feedTargetId==88);
        setup(2,360);inbox={from(77,QUEEN,unsigned(code({13,12}))|(5u<<12),359)};
        receiveRadio();assert(feedTargetId==77 && feedTargetLength==5);
        emitRadio();assert(outgoing.type==QUEEN && outgoing.stamp==359);
        cout<<"small queen reports relay; newer shrinkage defeats stale length passed\n";
    } else if(test=="far_donor") {
        setup(2,420);rememberAllyReport(77,10,{18,14},420);
        auto moves=legalMoves();Move chosen;string reason;
        assert(distance(tiles[24].p,feedTargetHead)>10);
        assert(!coordinateDonation(moves,chosen,reason));
        assert(role==1 && reason=="DONOR_SEEK" && donationTarget<0);
        cout<<"sonar length starts donor travel beyond the old ten-tile limit passed\n";
    } else if(test=="resource_share") {
        setup(2,100);tiles[local({12,10})].countdown=1;
        rememberTerrain();assert(resources.empty());
        roundNo++;rememberTerrain();assert(resources[code({12,10})].fountain);
        auto moves=legalMoves();assert(guideToResource(moves));
        emitRadio();assert(outgoing.type==PORTAL_ROUTE_PACKET && (outgoing.payload&(1u<<26)));
        auto report=outgoing;setup(2,103);
        inbox={from(77,report.type,report.payload,report.stamp)};receiveRadio();
        assert(resources[code({12,10})].stamp==101 && sharedPortalExit.empty());
        emitRadio();assert(outgoing.stamp==101);
        roundNo=162;inbox.clear();receiveRadio();assert(resources.empty());
        cout<<"fountain detection, relay provenance and expiry passed\n";
    } else if(test=="hunt_extended") {
        setup();units=4;ally(2,{{9,8},{8,8}},100);
        for(Pos p:vector<Pos>{{12,9},{12,8},{11,8},{11,7},{12,7},{13,7}}) {int j=local(p);tiles[j].id=99;tiles[j].team='B';tiles[j].head=p==Pos{12,9};}
        he[3][5]="w";rememberTerrain();
        huntEnemy=99;huntSince=88;huntProgress=99;
        received={{HUNT,2,100,unsigned(code({12,9}))|(99u<<12)|(3u<<24)|(1u<<26)}};
        auto moves=legalMoves();assert(coordinateHunt(moves));
        assert(outgoing.type==HUNT && int((outgoing.payload>>12)&4095)==99);
        assert(int((outgoing.payload>>24)&3)!=3);
        cout<<"productive hunt survives twelve turns and honors a teammate exit claim passed\n";
    } else if(test=="long_lookahead") {
        setup(8,420);
        for(int x=11;x<=12;x++) {he[3][x-7]="w";he[4][x-7]="w";}
        ve[3][6]="w";rememberTerrain();
        auto move=evaluate(1,local({11,10}));
        assert(!move.sustainable && !safeMove(move));
        cout<<"long queen rejects a move that closes after later tail steps passed\n";
    } else if(test=="queen_guard") {
        setup(2,420);units=6;feedTargetId=77;feedTargetLength=12;feedTargetHead={9,10};feedTargetStamp=420;
        for(Pos p:vector<Pos>{{11,10},{11,9},{12,9},{13,9},{13,8},{12,8}}) {
            int j=local(p);tiles[j].id=90;tiles[j].team='B';tiles[j].head=p==Pos{11,10};
        }
        assert(favorableHeadTrade()==1);
        role=1;assert(favorableHeadTrade()==1);role=6;assert(favorableHeadTrade()==1);
        feedTargetHead={2,2};assert(favorableHeadTrade()<0);
        cout<<"late short guards may trade against a large enemy threatening their queen passed\n";
    } else if(test=="tail_ambush") {
        setup(5,100);units=4;
        for(auto &t:tiles) if(t.id==id) {t.id=-1;t.head=false;}
        body={{10,10},{10,11},{10,12},{10,13},{10,14}};
        for(int k=0;k<5;k++) {
            int j=local(body[k]);if(j<0) continue;
            tiles[j].id=id;tiles[j].team=team;tiles[j].head=k==0;tiles[j].dir=0;
        }
        for(int y=10;y<=13;y++) {
            int j=local({11,y});tiles[j].id=20;tiles[j].team='B';tiles[j].head=y==13;tiles[j].dir=2;
        }
        auto moves=legalMoves();assert(tailAmbushEnemy(moves)==20);
        for(auto &t:tiles) if(t.id==20) t.id=5; // Enemy already acted: do not extrapolate twice.
        assert(tailAmbushEnemy(moves)<0);
        cout<<"tail split anticipates a later-ID enemy beside a remote tail passed\n";
    } else if(test=="ambush_child") {
        setup(2,100);id=30;units=3;turnsAlive=0;courierUntil=116;role=3;
        for(auto &t:tiles) if(t.id==10) t.id=id;
        ally(10,{{8,11},{8,10}},100);
        for(Pos p:vector<Pos>{{11,10},{11,9},{12,9},{13,9}}) {
            int j=local(p);tiles[j].id=99;tiles[j].team='B';tiles[j].head=p==Pos{11,10};
        }
        assert(newbornAmbush() && favorableHeadTrade()==1);
        int j=local({11,10});tiles[j].id=-1;tiles[j].head=false;
        assert(favorableHeadTrade()<0);
        cout<<"new two-segment child trades only with a still-adjacent enemy head passed\n";
    } else if(test=="queen_consent") {
        setup(2,400);officialQueenDeath=-1;rememberAllyReport(77,8,{12,10},400);
        assert(!donationCandidate());
        // A death certificate is compared to the queen's last life report,
        // not to a newer unrelated receiver report.
        inbox={from(0,QUEEN,unsigned(code({12,12})),398)};receiveRadio();
        assert(officialQueenId==0 && officialQueenDeath==398 && donationCandidate());
        setup(2,400);ally(0,{{12,10},{12,11},{12,12},{13,12},{13,11},{13,10},{13,9},{13,8}},400);
        inbox={from(0,QUEEN,unsigned(code({12,10})),398)};receiveRadio();
        assert(officialQueenDeath<0 && feedTargetId==0);
        cout<<"unknown queen blocks fallback gifts; valid death enables them; stale death cannot override life passed\n";
    } else if(test=="early_income") {
        setup(3,100);rememberAllyReport(0,5,{12,10},100);
        int pearl=local({11,10});tiles[pearl].pearl=true;
        assert(!reservedPearl(code(tiles[pearl].p)));
        auto moves=legalMoves();assert(any_of(moves.begin(),moves.end(),[&](const Move&m){return m.to==pearl && safeMove(m);}));
        roundNo=350;rememberAllyReport(0,5,{12,10},350);
        assert(reservedPearl(code(tiles[pearl].p)));
        moves=legalMoves();assert(none_of(moves.begin(),moves.end(),[&](const Move&m){return m.to==pearl;}));
        cout<<"queen region permits early scout food and reserves it during late feeding passed\n";
    } else if(test=="donor_sprint") {
        setup(6,360);turnsAlive=40;units=6;
        for(auto &t:tiles) if(t.id==id) t=Tile{t.p};
        body={{10,10},{9,10},{8,10},{8,11},{9,11},{9,12}};
        for(int k=0;k<int(body.size());k++) {int j=local(body[k]);tiles[j].id=id;tiles[j].team=team;tiles[j].head=k==0;}
        rememberAllyReport(0,8,{16,10},roundNo);role=1;
        auto m=legalMoves();auto it=find_if(m.begin(),m.end(),[](const Move&m){return m.dir==1;});assert(it!=m.end());
        Move first=*it;int second=donorSprint(first,"DONOR_SEEK");assert(second==1);
        int to=nextTile(first.to,second,false);recordMove(first);Move step;step.dir=second;step.to=to;recordMove(step);
        assert(body.front()==Pos({12,10}) && body.size()==6); // ceil(6/4)=2 free steps.
        setup(4,360);rememberAllyReport(0,8,{16,10},roundNo);role=1;
        m=legalMoves();assert(donorSprint(m.front(),"DONOR_SEEK")<0);
        setup(6,360);id=0;role=1;assert(donorSprint(first,"DONOR_SEEK")<0);
        setup(6,360);rememberAllyReport(0,8,{16,10},roundNo);role=1;
        int j=local({12,10});tiles[j].id=22;tiles[j].team=team;
        assert(donorSprint(first,"DONOR_SEEK")<0);
        cout<<"late donor takes two free verified steps; queen, small and crowded cases reject sprint passed\n";
    } else if(test=="fallback_feeding") {
        setup(2,350);rememberAllyReport(77,4,{12,12},roundNo);
        officialQueenDeath=-1;
        assert(feedTargetId==77 && !donationCandidate());
        officialQueenDeath=0;assert(donationCandidate());
        rememberAllyReport(0,3,{13,13},roundNo);
        assert(feedTargetId==0 && donationCandidate()); // Fixed queen always outranks length-4 fallback.
        assert(eligibleDonor(id,12) && !eligibleDonor(id,13)); // Larger collectors may still feed the scoring queen.
        cout<<"small fallback king can receive gifts; fixed queen retains absolute priority passed\n";
    } else if(test=="corridor_gift") {
        setup(2,420);ally(0,{{12,10},{12,11},{12,12},{13,12},{13,11},{13,10},{13,9},{13,8}},420);
        int h=visibleHead(0);he[h/7][h%7]="w";ve[h/7][h%7+1]="w";
        assert(collectionPlan(id,0,8).safe); // West remains safe; south is the queen's own body.
        int j=local({11,10});tiles[j].id=90;tiles[j].team='B';tiles[j].head=true;
        assert(!collectionPlan(id,0,8).safe);
        cout<<"one-exit corridor admits a recoverable gift but rejects enemy contact passed\n";
    } else return 2;
    return 0;
}

};

} // namespace battlecode

int main(int argc, char **argv) {
    static battlecode::BotTestAccess bot;
    return bot.execute(argc, argv);
}
