#include<cstdio>
#include<list>
#include<forward_list>
#include<string>
#include<unordered_map>
#include"../cmdhelper.h"
#include<vector>
#include"minecraft/level/Level.h"
#include"minecraft/actor/Player.h"
#include"minecraft/actor/ItemActor.h"
#include"minecraft/core/GameMode.h"
#include"minecraft/item/ItemStack.h"
#include"seral.hpp"
#include <sys/stat.h>
#include<unistd.h>
#include <sys/stat.h>
#include"money.h"
#include"base.h"
#include<cmath>
#include<deque>
#include<dlfcn.h>
using std::string;
using std::deque;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define dbg_printf(...) {}
//#define dbg_printf printf
static int dummy(){return 0;}
extern "C" {
    void land_init(std::list<string>& modlist);
}
extern void load_helper(std::list<string>& modlist);
typedef float R;
#define PERMB 1
#define PERMU 2
#define PERMP 4
#define PERMA 8
struct land {
    int dim_perm;
    int x,y;
    int dx,dy;
    string owner;
    int size() {
        return dx*dy;
    }
    bool inland(int xx,int yy,int di) const {
        return di==(dim_perm>>4) && xx>=x && yy>=y && xx-x<dx && yy-y<dy;
    }
    bool checkown(const string& name) const {
        return owner.find("|"+name+"|")!=std::string::npos;
    }
    bool canpop(const string& name) const {
        return checkown(name) || ((dim_perm&PERMP)!=0);
    }
    bool canuse(const string& name) const {
        return checkown(name) || ((dim_perm&PERMU)!=0);
    }
    bool canbuild(const string& name) const {
        return checkown(name) || ((dim_perm&PERMB)!=0);
    }
    bool canatk(const string& name) const {
        return checkown(name) || ((dim_perm&PERMA)!=0);
    }
    bool coll(int xb1,int xb2,int yb1,int yb2,int dim,const string& nm) const {
        if((dim_perm>>4)==dim) {
            int xa1=this->x,ya1=this->y,xa2=this->x+dx-1,ya2=this->y+dy-1;
            int t1x=max(xa1,xb1);
            int t1y=max(ya1,yb1);
            int t2x=min(xa2,xb2);
            int t2y=min(ya2,yb2);
            return t2x>=t1x && t2y>=t1y && !checkown(nm);
        }
        return false;
    }
    void addown(const string& name) {
        owner+="|"+name+"|";
    }
    void rmown(const string& n) {
        string fk="|"+n+"|";
        auto i=owner.find(fk);
        if(i!=string::npos) owner.erase(i,i+fk.size());
    }
    static string tostr(const land& a) {
        char buf[32768];
        int ptr=0;
        putint(buf,ptr,a.dim_perm);
        putint(buf,ptr,a.x);
        putint(buf,ptr,a.y);
        putint(buf,ptr,a.dx);
        putint(buf,ptr,a.dy);
        putstr(buf,ptr,a.owner);
        return string(buf,ptr);
    }
    static void fromstr(const string& b,land& a) {
        const char* buf=b.c_str();
        int ptr=0;
        getint(buf,ptr,a.dim_perm);
        getint(buf,ptr,a.x);
        getint(buf,ptr,a.y);
        getint(buf,ptr,a.dx);
        getint(buf,ptr,a.dy);
        a.owner=getstr(buf,ptr);
    }
};

static std::forward_list<land> lands;
static std::unordered_map<string,Vec3> startpos,endpos;
static void save();
static land* getLand_r(int x,int z,int dim) {
    for(auto &i:lands) {
        if(i.inland(x,z,dim)) {
            return &i;
        }
    }
    return NULL;
}
struct lc_ent {
    int x,z,dim;
    land* v;
};
lc_ent cpool_2[100];
std::list<int> cpool;
static void cache_init(){
	for(int i=0;i<100;++i) cpool_2[i].dim=4,cpool.push_back(i);
}
static void ccache() {
    cpool.clear();
    cache_init();
}
static bool cache_fetch(land** res,int x,int z,int dim) {
    int fg=0;
    int& found=fg; //dummy
    for(auto &i_ :cpool) {
	auto const &i=cpool_2[i_];
        if(i.x==x && i.z==z && i.dim==dim) {
            *res=i.v;
            found=i_;
            fg=1;
            break;
        }
    }
    if(fg) std::swap(cpool.front(),found);
    return fg;
}
static land* cache_push(const lc_ent& en) {
    int x=cpool.back();
    cpool.pop_back();
    cpool_2[x]=en;
    cpool.push_front(x);
    return en.v;
}
land* getLand(int x,int z,int dim) {
    land* res;
    if(cache_fetch(&res,x,z,dim)) return res;
    return cache_push({x,z,dim,getLand_r(x,z,dim)});
}
static void oncmd(std::vector<string>& a,CommandOrigin const & b,CommandOutput &outp) {
    int pl=(int)b.getPermissionsLevel();
    string name=b.getName();
    ARGSZ(1)
    if(a[0]=="start") {
        startpos[name]=b.getWorldPosition();
        outp.success("§e[Land system] 已选择点A，输入/land end选择另一个点");
    }
    if(a[0]=="end") {
        endpos[name]=b.getWorldPosition();
        outp.success("§e[Land system] 已选择点B");
        if(startpos.count(name)+endpos.count(name)!=2) {
            outp.error("[Land system] 请选择开始和结束点");
        }
        Vec3 pa=startpos[name];
        Vec3 pb=endpos[name];
        int xx(round(min(pa.x,pb.x))),yy(round(min(pa.z,pb.z))),xx2(round(max(pa.x,pb.x))),yy2(round(max(pa.z,pb.z)));
        int fg=0;
        for(auto const& i:lands) {
            if(i.coll(xx,xx2,yy,yy2,b.getEntity()->getDimensionId(),name) && pl==0) {
                fg=1;
                break;
            }
        }
        if(fg) {
            outp.error("[Land system] 领地不得重叠");
            startpos.erase(name);
            endpos.erase(name);
            return;
        }
        land tmp;
        tmp.x=xx,tmp.y=yy,tmp.dx=xx2-xx+1,tmp.dy=yy2-yy+1;
        int price=10*tmp.size();
        char buf[1000];
        sprintf(buf,"§e[Land system] 选择完毕，该区域价格 %d,使用“/land buy”购买",price);
        outp.success(string(buf));
    }
    if(a[0]=="buy") {
        if(startpos.count(name)+endpos.count(name)!=2) {
            outp.error("[Land system] 请选择开始和结束点");
        }
        Vec3 pa=startpos[name];
        Vec3 pb=endpos[name];
        int xx(min(pa.x,pb.x)),yy(min(pa.z,pb.z)),xx2(max(pa.x,pb.x)),yy2(max(pa.z,pb.z)); //fix
        land tmp;
        tmp.x=xx,tmp.y=yy,tmp.dx=xx2-xx+1,tmp.dy=yy2-yy+1;
        tmp.addown(name);
        tmp.dim_perm=(b.getEntity()->getDimensionId()<<4)|PERMP;
        int price=10*tmp.size();
        if(pl>0 || red_money(name,price)) {
            lands.push_front(tmp);
            ccache();
            //save();
            outp.success("§e[Land system] 购买成功");
        } else {
            outp.error("[Land system] 你的余额不足");
        }
    }
    if(a[0]=="query") {
        auto act=b.getEntity();
        if(act==NULL) return;
        int dim=act->getDimensionId();
        int x=round(b.getWorldPosition().x);
        int y=round(b.getWorldPosition().z); //fix
        /* for(auto const& i:lands){
             if(i.inland(x,y,dim) && i.canpop(name)){
                 char buf[1000];
                 sprintf(buf,"This is %s's land\n",i.owner.c_str());
                 outp.success(string(buf));
                 return;
             }
         }*/
        land* i=getLand(x,y,dim);
        if(i && i->canpop(name)) {
            char buf[1000];
            sprintf(buf,"§e[Land system] 这里是 %s 的领地",i->owner.c_str());
            outp.success(string(buf));
            //sendText2(getplayer_byname(name),string(buf));
            return;
        } else {
            char buf[1000];
            sprintf(buf,"[Land system] 这里没有领地");
            outp.error(string(buf));
        }
    }
    if(a[0]=="sell") {
        int dim=b.getEntity()->getDimensionId();
        int x=round(b.getWorldPosition().x);
        int y=round(b.getWorldPosition().z); //fix
        //for(auto& i:lands){
        //   if(i.inland(x,y,dim) && i.checkown(name)){
        land* i=getLand(x,y,dim);
        if(i&&(i->checkown(name)||pl>0)) {
            char buf[1000];
            int price=0*i->size();
            add_money(name,price);
            sprintf(buf,"§e[Land system] 领地出售成功，出售价格 %d",price);
            outp.success(string(buf));
            i->dim_perm=114514; //remove flag
            lands.remove_if([](const land& n) {
                return n.dim_perm==114514;
            });
            ccache();
            //save();
            return;
        } else {
            outp.error("[Land system] 这不是你的领地");
        }
    }
    if(a[0]=="trust") {
        ARGSZ(2)
        int dim=b.getEntity()->getDimensionId();
        int x=round(b.getWorldPosition().x);
        int y=round(b.getWorldPosition().z); //fix
        land* i=getLand(x,y,dim);
        if(i&&(i->checkown(name)||pl>0)) {
            i->addown(a[1]);
            //save();
            outp.success("§e[Land system] 已信任玩家 "+a[1]);
            return;
        } else {
            outp.error("[Land system] 这不是你的领地");
        }
    }
    if(a[0]=="untrust") {
        ARGSZ(2)
        int dim=b.getEntity()->getDimensionId();
        int x=round(b.getWorldPosition().x);
        int y=round(b.getWorldPosition().z); //fix
        land* i=getLand(x,y,dim);
        if(i&&(i->checkown(name)||pl>0)) {
            i->rmown(a[1]);
            //save();
            outp.success("§e[Land system] 已取消玩家 "+a[1]+" 的信任");
            return;
        } else {
            outp.error("[Land system] 这不是你的领地");
        }
    }
    if(a[0]=="perm") {
        ARGSZ(2)
        int dim=b.getEntity()->getDimensionId();
        int x=round(b.getWorldPosition().x);
        int y=round(b.getWorldPosition().z); //fix
        land* i=getLand(x,y,dim);
        if(i&&(i->checkown(name)||pl>0)) {
            i->dim_perm-=(i->dim_perm&15);
            i->dim_perm|=atoi(a[1].c_str());
            //save();
            outp.success("§e[Land system] 权限更改 "+a[1]);
        } else {
            outp.error("[Land system] 这不是你的领地");
        }
    }

    if(a[0]=="help") {
        outp.error("领地系统指令列表:\n/land start ——选择起点（你站的地方）\n/land end ——选择终点\n/land buy ——选点之后买地（1格10块）\n/land trust 玩家ID ——添加访客\n/land untrust 玩家ID ——删除访客\n/land sell ——卖地\n/land query ——查看当前领地主人\n/land perm 数字 ——指定具体权限(详细看github)");
    }
}

static void save() {
    char* bf;
    int sz=settomem(lands,&bf,land::tostr);
    mem2file("data/land/land.db",bf,sz);
}
static void load() {
    mkdir("data",S_IRWXU);
    mkdir("data/land",S_IRWXU);
    register_shutdown(fp(save));
    char* buf;
    int sz;
    struct stat tmp;
    if(stat("data/land/land.db",&tmp)==-1) {
        save();
    }
    file2mem("data/land/land.db",&buf,sz);
    memtoset(lands,buf,land::fromstr);
}

static int handle_dest(GameMode* a0,BlockPos const& a1,unsigned char a2) {
    int pl=a0->getPlayer()->getPlayerPermissionLevel();
    if(pl>1) {
        //op
        //printf("op\n");
        return 1;
    }
    const string& name=a0->getPlayer()->getName();
    int dim=a0->getPlayer()->getDimensionId();
    int x(a1.x),y(a1.z),z(a1.z); //fixed
    dbg_printf("use handle dim %d %d %d %d",dim,x,y,z);
    land* i=getLand(x,y,dim);
    if(i&&!i->checkown(name)) {
        if(!i->canbuild(name)) {
            char buf[1000];
            sprintf(buf,"§c你不能破坏 %s 的领地",i->owner.c_str());
            sendText2(a0->getPlayer(),string(buf));
            return 0;
        } else {
            return 1;
        }
    }
    return 1;
}
static int handle_atk(Player* a0,Actor & a1) {
    int pl=a0->getPlayerPermissionLevel();
    if(pl>1) {
        //op
        return 1;
    }
    const string& name=a0->getName();
    int dim=a1.getDimensionId();
    Vec3 vc=a1.getPos();
    int x(vc.x),y(vc.z); //fixed
    land* i=getLand(x,y,dim);
    if(i&&!i->checkown(name)) {
        if(!i->canatk(name)) {
            char buf[1000];
            sprintf(buf,"§c你不可以在 %s 的领地内攻击",i->owner.c_str());
            sendText2(a0,string(buf));
            return 0;
        } else {
            return 1;
        }
    }
    return 1;
}
/*
int handle_bui(GameMode* a0,BlockPos const& a1,unsigned char a2){
    int pl=a0->getPlayer()->getPlayerPermissionLevel();
    if(pl==2){
        //op
        return 1;
    }
    string& name=a0->getPlayer()->getName();
    int dim=a0->getPlayer()->getDimensionId();
    int x(a1.x),y(a1.y),z(a1.z);
    dbg_printf("bui handle dim %d %d %d %d",dim,x,y,z);
            if(i&&!i->checkown(name)){
                if(!i->canbuild(name)){
                    return 0;
                }else{
                    return 1;
                }
            }
    return 1;
}*/
static int handle_useion(GameMode* a0,ItemStack & a1,BlockPos const& a2,unsigned char a3,Vec3 const& a4,Block const* a5) {
    int pl=a0->getPlayer()->getPlayerPermissionLevel();
    if(pl>1) {
        //op
        return 1;
    }
    const string& name=a0->getPlayer()->getName();
    int dim=a0->getPlayer()->getDimensionId();
    int x(a2.x),y(a2.z),z(a2.z); //fixed
    /*
    char fku[555];
        sprintf(fku,"use handle %d %d a3 %d\n",x,z,a3);
    sendText(a0->getPlayer(),string(fku));
    */
    const int Z[]= {-1,1,0,0};
    const int X[]= {0,0,-1,1};
    if(a3>1 && a3<6) x+=X[a3-2],y+=Z[a3-2];
    /*for(auto const& i:lands){
        if(i.inland(x,y,dim)){
            if(!i.canuse(name)){
                return 0;
            }else{
                if(a1.isNull() || i.checkown(name))
                    return 1;
                else
                {
                    return 0;
                }

            }
        }
    }*/
    land* i=getLand(x,y,dim);
    if(i&&!i->checkown(name)) {
        if(!i->canuse(name)) {
            char buf[1000];
            sprintf(buf,"§c你不能在 %s 的领地与方块互交",i->owner.c_str());
            sendText2(a0->getPlayer(),string(buf));
            return 0;
        } else {
            return i->checkown(name) || a1.isNull();
        }
    }
    return 1;
}

void land_init(std::list<string>& modlist) {
    printf("[LAND] loaded!\n");
    load();
    cache_init();
    MyHook(fp(dlsym(NULL,"_ZNK9FarmBlock15transformOnFallER11BlockSourceRK8BlockPosP5Actorf")),fp(dummy));
    register_cmd("land",(void*)oncmd,"领地系统");
    reg_destroy(fp(handle_dest));
    //reg_build(fp(handle_bui));
    reg_useitemon(fp(handle_useion));
    reg_attack(fp(handle_atk));
    load_helper(modlist);
}
