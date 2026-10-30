#pragma once
#include "worlded/assets.hpp"
#include <array>
#include <deque>
#include <functional>
#include <set>
#include <algorithm>

namespace worlded {
// The original WorldED uses the tilename.mes edge graph (400+) to insert
// intermediate terrains. SEC tiles encode two materials and four corner bits.
// Coordinates: north=1, west=2, south=4, east=8 in the isometric view.
class TerrainBrush {
    std::map<int,std::set<int>> edges;
    std::map<std::string,int> names;
public:
    explicit TerrainBrush(const Installation& installation) {
        auto messages=read_messages(installation.read("art/tile/tilename.mes"));
        for(const auto& m:messages)if(m.id<200){auto name=m.text.substr(0,3);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});names[name]=m.id;edges[m.id];}
        for(const auto& m:messages)if(m.id>=400 && m.text.size()>=6){auto name=m.text.substr(0,6);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
            if(names.count(name.substr(0,3)) && names.count(name.substr(3,3))){int a=names[name.substr(0,3)],b=names[name.substr(3,3)];edges[a].insert(b);edges[b].insert(a);}}
    }
    static int material(std::uint32_t aid,bool first) {return ((aid>>(first?22:16))&63)+((aid&256)?0:200)+((aid&(first?128:64))?0:100);}
    static std::array<int,4> corners(std::uint32_t aid) {
        int mask=(aid>>12)&15;if(aid&1)mask=(mask&5)|((mask&2)<<2)|((mask&8)>>2);
        std::array<int,4> result;for(unsigned i=0;i<4;i++)result[i]=material(aid,mask&(1<<i));return result;
    }
    static std::uint32_t compose(int first,int second,int mask,unsigned variant) {
        constexpr int canonical[]={0,1,8,3,4,5,6,7,8,3,10,11,6,7,14,15};
        bool outdoors=first<200,flip1=first%200<100,flip2=second%200<100;
        unsigned flip=flip1 && flip2 && canonical[mask]!=mask;if(flip)mask=canonical[mask];
        return ((first%100u)<<22)|((second%100u)<<16)|(mask<<12)|(variant<<9)|(outdoors?256:0)|(flip1?128:0)|(flip2?64:0)|flip;
    }
    Sector paint(const Sector& original,const std::set<std::pair<int,int>>& cells,std::uint32_t brush,const std::function<bool(std::uint32_t)>& available) const {
        int target=material(brush,true);if(target>=200)throw std::runtime_error("Automatic transitions apply to outdoor terrain. Disable transitions for indoor floors.");
        std::map<int,int> parent,depth;std::deque<int> graph;parent[target]=target;depth[target]=0;graph.push_back(target);
        while(!graph.empty()){int n=graph.front();graph.pop_front();auto found=edges.find(n);if(found==edges.end())continue;for(int next:found->second)if(!parent.count(next)){parent[next]=n;depth[next]=depth[n]+1;graph.push_back(next);}}
        std::array<int,65*65> before,vertices;before.fill(-1);std::array<bool,65*65> changed{};
        const int dx[]={0,1,1,0},dy[]={0,0,1,1};
        for(int y=0;y<64;y++)for(int x=0;x<64;x++){auto c=corners(original.tile(x,y));for(int i=0;i<4;i++)if(before[(x+dx[i])+65*(y+dy[i])]<0)before[(x+dx[i])+65*(y+dy[i])]=c[i];}
        vertices=before;std::deque<int> pending;
        // Revisit even corners already in the target material. Imported or
        // manually placed tiles can have sharp, inconsistent shared corners;
        // repainting their boundary must rebuild the neighboring transitions.
        for(auto [x,y]:cells){if(x<0 || x>=64 || y<0 || y>=64)throw std::runtime_error("Brush outside sector");for(int i=0;i<4;i++){int v=x+dx[i]+65*(y+dy[i]);vertices[v]=target;changed[v]=true;pending.push_back(v);}}
        while(!pending.empty()) {
            int v=pending.front();pending.pop_front();int x=v%65,y=v/65;
            for(int j=-1;j<=1;j++)for(int i=-1;i<=1;i++)if(x+i>=0 && x+i<=64 && y+j>=0 && y+j<=64){int n=x+i+65*(y+j);int a=vertices[v],b=vertices[n];if(a==b || (edges.count(a) && edges.at(a).count(b)))continue;
                if(!parent.count(a) || !parent.count(b))throw std::runtime_error("No transition is available between these materials. Disable transitions at this boundary.");
                int change=depth[a]>depth[b]?v:n;vertices[change]=parent[vertices[change]];changed[change]=true;pending.push_back(change);pending.push_back(v);
            }
        }
        for(int i=0;i<65;i++)if(vertices[i]!=before[i] || vertices[65*64+i]!=before[65*64+i] || vertices[65*i]!=before[65*i] || vertices[65*i+64]!=before[65*i+64])
            throw std::runtime_error("Transitions reach the sector boundary. Reduce the brush size or paint farther from the edge; neighboring sectors are not modified.");
        auto result=original;
        for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
            bool touched=false;std::array<int,4> c;for(int i=0;i<4;i++){int v=x+dx[i]+65*(y+dy[i]);touched|=changed[v];c[i]=vertices[v];}if(!touched)continue;
            std::set<int> types(c.begin(),c.end());if(types.size()>2)throw std::runtime_error("A transition between more than two materials cannot be represented.");
            int a=*types.begin(),b=*types.rbegin(),mask=0;for(int i=0;i<4;i++)if(c[i]==a)mask|=1<<i;
            std::vector<unsigned> variants;for(unsigned variant=0;variant<8;variant++){auto aid=compose(a,b,mask,variant);if(available(aid))variants.push_back(aid);}
            if(variants.empty())throw std::runtime_error("The transition variant is missing from the game resources; the stroke was cancelled.");
            result.set_tile(x,y,variants[(x*73856093u^y*19349663u)%variants.size()]);
        }
        return result;
    }
};
}
