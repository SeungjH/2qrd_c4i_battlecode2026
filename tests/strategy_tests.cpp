#define main battlecode_main
#include "../main.cpp"
#undef main
#include <cassert>

void setup(int len=3,int r=100) {
    width=height=20; id=10; team='A'; length=len; roundNo=r; units=2; unitLimit=64; facing=bearing=1;
    localAt.fill(-1); lastSeen.fill(-1); lastVisit.fill(-10000); pearlSeen.fill(-1);
    for(auto &v:terrain) v.fill(-2);
    for(auto &v:portalAt) v.fill(-1);
    portalPairs.clear(); knownPortalEdges.clear(); portalLastUsed.clear();
    sharedPortalExit.clear();portalRouteLastSent.clear();warnings.clear();allies.clear();received.clear();
    warningSequence=0;
    queenId=-1;queenLength=0;queenStamp=-100;queenLastSent=-100;hasOutgoing=false;
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
    rememberQueen(owner,int(b.size()),b[0],stamp);
}
unsigned long long from(int owner,int type,unsigned data,int stamp) {
    int saved=id;id=owner;auto raw=encodePacket({type,owner,stamp,data});id=saved;return raw;
}
int main(int argc,char**argv) {
    assert(argc==2); string test=argv[1];
    if(test=="short_loop") {
        setup();turnsAlive=40;visits[code(tiles[24].p)]=1;
        for(int j=0;j<24;j++) recentHeads.push_back(code({j%20,j/20}));
        vector<Pos> cycle{{10,9},{11,9},{11,10},{10,10}};
        for(int j=0;j<7;j++) recentHeads.push_back(code(cycle[j%4]));
        auto m=legalMoves();assert(guideOutOfLoop(m));assert(navigating && goal>=0);
        for(Pos p:cycle) assert(goal!=code(p));
        assert(safeMove(m.front()));
        cout<<"two laps of a four-cell loop choose a reachable destination outside the loop passed\n";
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
        setup(2,400);ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8}},400);
        auto moves=legalMoves();Move chosen;string reason="EXPLORE";
        assert(collectionPlan(id,2,8).safe);
        assert(!coordinateDonation(moves,chosen,reason));assert(chosen.dir<0 && outgoing.type==OFFER);
        int stamp=offerStamp;
        received={{ACCEPT,2,400,unsigned(id)|(unsigned(stamp)<<16)}};
        assert(coordinateDonation(moves,chosen,reason));
        assert(reason=="DONATE_CONFIRMED" && chosen.dir==3); // own tail, not another head
        chosen=Move{};roundNo=349;assert(!coordinateDonation(moves,chosen,reason));
        roundNo=401;received[0].payload=11;assert(!coordinateDonation(moves,chosen,reason));
        cout<<"queen donation needs correct ACK, round >=350 and safe collection passed\n";
    } else if(test=="contested") {
        setup(2,400);ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8}},400);
        int j=local({11,11});tiles[j].id=99;tiles[j].head=true;tiles[j].team='B';
        assert(!collectionPlan(id,2,8).safe);
        auto moves=legalMoves();Move chosen;string reason;
        assert(!coordinateDonation(moves,chosen,reason));assert(donationTarget<0);
        cout<<"contested pearls block deliberate sacrifice passed\n";
    } else if(test=="receiver") {
        setup(8,400);rememberQueen(id,length,tiles[24].p,roundNo);
        ally(20,{{11,9},{12,9}},400);ally(21,{{12,11},{13,11}},400);
        received={{OFFER,20,400,unsigned(id)},{OFFER,21,400,unsigned(id)}};
        auto m=legalMoves();Move c;string reason;
        coordinateDonation(m,c,reason);
        assert(receiveDonor==20 && role==2 && outgoing.type==ACCEPT && int(outgoing.payload&65535)==20);
        assert(donationTarget<0);
        cout<<"receiver reserves one donor, never simultaneously donates passed\n";
    } else if(test=="late_donation") {
        setup(2,410);
        ally(2,{{11,9},{12,9},{13,9},{13,8},{12,8},{11,8},{10,8},{9,8},{9,9},{8,9}},410);
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
        setup();units=4;ally(2,{{10,8},{9,8}},100);
        int enemy=local({12,9});tiles[enemy].id=99;tiles[enemy].team='B';tiles[enemy].head=true;
        received.clear(); // Visibility suffices; no received HUNT claim is needed.
        auto m=legalMoves();assert(coordinateHunt(m));assert(role==4 && outgoing.type==HUNT);
        assert(safeMove(m.front()));
        cout<<"two visible allies coordinate without a sonar handshake passed\n";
    } else if(test=="hunt_timeout") {
        setup();units=4;ally(2,{{9,8},{8,8}},100);
        int enemy=local({12,10});tiles[enemy].id=99;tiles[enemy].team='B';tiles[enemy].head=true;
        huntEnemy=99;huntSince=88;huntProgress=99;
        received={{HUNT,2,100,99}};
        auto m=legalMoves();assert(!coordinateHunt(m));assert(huntEnemy<0 && huntCooldown==108);
        cout<<"hunting stops after twelve rounds passed\n";
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
        assert(moves.empty() && !cutCanReleaseParent() && reverseRescue(moves));
        assert(length-2==8 && local(body.back())>=0);
        cout<<"trapped long parent leaves old head 2 and reverses 8 safe tail segments passed\n";
    } else if(test=="roles_and_income") {
        setup(4,100);turnsAlive=30;foodRounds.push_back(99);
        auto moves=legalMoves();assert(routineSplit(moves));
        roundNo=320;foodRounds.push_back(319);assert(routineSplit(moves));
        roundNo=420;assert(!routineSplit(moves));
        foodRounds.push_back(419);assert(routineSplit(moves));
        setup(8,420);assert(protectedLong());
        auto rich=legalMoves();assert(!routineSplit(rich));
        setup(5,320);foodRounds={300,319};
        auto productive=legalMoves();assert(routineSplit(productive));
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
        assert(portalUses==6 && !portalReady(7) && body.empty());
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
        queenId=99;queenLength=12;queenHead={15,12};queenStamp=410;
        assert(!coordinateDonation(moves,chosen,reason));
        assert(donationTarget<0 && (!hasOutgoing || outgoing.type!=OFFER));
        int to=local({10,11});tiles[to].pearl=1;
        queenHead={10,12};double reserved=evaluate(2,to).score;
        queenId=id;double queenScore=evaluate(2,to).score;
        assert(reserved+90<queenScore);
        queenId=2;queenLength=8;queenHead={11,9};queenStamp=410;
        assert(!coordinateDonation(moves,chosen,reason));
        assert(donationTarget==2 && outgoing.type==OFFER);
        cout<<"only the elected queen may receive an intentional donation passed\n";
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
        assert(queenId==77 && queenLength==20 && queenHead==Pos(13,12));
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
    } else return 2;
}
