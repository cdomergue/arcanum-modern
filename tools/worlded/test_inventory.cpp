#include "src/inventory_editor.hpp"
#include <iostream>
#include <fstream>

using namespace worlded;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> static void rejects(F f,const char* message){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
static GameObject object(unsigned type,unsigned prototype){GameObject base;base.type=type;base.prototype_id=prototype;auto result=clone_instance(base);set_number(result,"LOCATION",12ull|(18ull<<32));return result;}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    try{
        auto owner=object(16,17000),sword=object(5,6000),gold=object(8,8000),untouched=object(3,4000);
        set_number(owner,"CRITTER_INVENTORY_SOURCE",123);set_number(sword,"ITEM_INV_LOCATION",1004);set_number(gold,"ITEM_INV_LOCATION",0);set_number(gold,"GOLD_QUANTITY",321);
        auto resolve=[](const GameObject& o){return o;};auto size=[](const GameObject&){return std::pair<unsigned,unsigned>{1,1};};
        auto edited=inventory_apply({owner,untouched},owner.id,{sword,gold},resolve,size);
        check(edited.size()==4,"Inventory omitted its MOB objects");auto contents=inventory_contents(edited,edited[0]);check(contents.size()==2 && contents[0].id==sword.id,"Inventory GUID order changed");check(edited[0].number("CRITTER_INVENTORY_SOURCE")==0,"Manual inventory still uses generator");check(edited[0].field("CRITTER_GOLD")->text==gold.id,"Gold cache reference missing");check(encode_instance(edited[1])==encode_instance(untouched),"Unrelated object changed");
        std::vector<GameObject> roundtrip;for(const auto& o:edited)roundtrip.push_back(read_objects(encode_instance(o))[0]);
        check(inventory_contents(roundtrip,roundtrip[0])[1].number("GOLD_QUANTITY")==321,"Quantity/reference lost on native encode/read");check(inventory_parent(roundtrip[2])==owner.id,"Parent handle lost on native encode/read");
        auto ids=inventory_descendants(edited,{owner.id});check(ids.size()==3 && ids.count(gold.id),"Copy/delete did not include owned items");std::map<std::string,std::string> mapping;for(const auto& o:edited)if(ids.count(o.id))mapping[o.id]=clone_instance(o).id;
        auto copy=edited;for(auto& o:copy)if(mapping.count(o.id)){auto id=mapping.at(o.id);inventory_remap(o,mapping);o.id=id;}
        check(inventory_contents(copy,copy[0])[1].id==mapping.at(gold.id),"Copied inventory references originals");
        auto removed=inventory_apply(edited,owner.id,{contents[0]},resolve,size);check(removed.size()==3 && !removed[0].field("CRITTER_GOLD")->present,"Removed inventory item/cache survived");
        auto duplicate=sword;duplicate=clone_instance(duplicate);rejects([&]{inventory_apply({owner},owner.id,{sword,duplicate},resolve,size);},"Duplicate equipment slot accepted");
        auto bagSword=sword;set_number(bagSword,"ITEM_INV_LOCATION",0);rejects([&]{inventory_apply({owner},owner.id,{bagSword,gold},resolve,size);},"Overlapping inventory cells accepted");
        set_number(bagSword,"ITEM_INV_LOCATION",119);rejects([&]{inventory_apply({owner},owner.id,{bagSword},resolve,[](const auto&){return std::pair<unsigned,unsigned>{2,2};});},"Item beyond grid edge accepted");
        auto missing=edited;missing.pop_back();rejects([&]{inventory_contents(missing,missing[0]);},"Missing inventory MOB silently dropped");
        auto chest=object(2,3000);rejects([&]{inventory_apply({chest},chest.id,{sword},resolve,size);},"Chest equipped a weapon");
        auto shield=object(7,7000);set_number(shield,"ITEM_INV_AID",0x60004002);set_number(shield,"ITEM_INV_LOCATION",1005);set_number(sword,"WEAPON_FLAGS",12);rejects([&]{inventory_apply({owner},owner.id,{sword,shield},resolve,size);},"Fixed two-handed weapon accepted alongside shield");
        if(argc==4 && std::string(argv[1])=="--housing-fixture"){
            auto installation=std::make_shared<Installation>(std::filesystem::u8path(argv[2]));SceneAssets assets(installation);auto catalog=assets.catalog();auto make=[&](unsigned type,unsigned proto,unsigned index){
                auto entry=std::find_if(catalog.begin(),catalog.end(),[&](const auto& e){return e.object.type==type && (!proto || e.object.prototype_id==proto);});check(entry!=catalog.end(),"Native inventory fixture prototype missing");auto obj=clone_instance(entry->object);
                auto id=inventory_id_bytes(obj.id);for(unsigned n=8;n<24;n++)id[n]=static_cast<unsigned char>(index*16+n-8);obj.id=inventory_bytes_id(id);obj.editor_id=obj.id;
                set_number(obj,"FLAGS",0x2000);set_number(obj,"LOCATION",(1346ull*64+30)|((1024ull*64+25+index%3)<<32));set_null(obj,"SCRIPTS_IDX");return obj;};
            auto chest=make(2,3044,2),coins=make(8,9076,3),dagger=make(5,6049,4),npc=make(16,17305,5),wielded=make(5,6049,6);
            set_number(coins,"ITEM_INV_LOCATION",0);set_number(coins,"GOLD_QUANTITY",54321);set_number(dagger,"ITEM_INV_LOCATION",10);set_number(wielded,"ITEM_INV_LOCATION",1004);
            set_number(npc,"CRITTER_TELEPORT_DEST",npc.number("LOCATION"));set_number(npc,"NPC_STANDPOINT_DAY",npc.number("LOCATION"));set_number(npc,"NPC_STANDPOINT_NIGHT",npc.number("LOCATION"));
            auto resolve=[&](const auto& o){return assets.effective(o);};auto scene=inventory_apply({chest,npc},chest.id,{coins,dagger},resolve,size);scene=inventory_apply(scene,npc.id,{wielded},resolve,size);
            auto light=make(3,0,7);set_number(light,"FLAGS",0x2500);set_number(light,"SCENERY_FLAGS",0);set_number(light,"AID",0x40000000);set_number(light,"CURRENT_AID",0x40000000);set_number(light,"LIGHT_AID",0x90000000);set_number(light,"LIGHT_FLAGS",0);set_number(light,"LIGHT_COLOR",0xd25a1e);
            ObjectField overlays;overlays.name="OVERLAY_LIGHT_AID";overlays.kind="UINT32_ARRAY";for(unsigned i=0;i<4;i++)overlays.elements.push_back({i,Bytes(4,255)});light.fields.push_back(overlays);scene.push_back(light);
            auto directory=std::filesystem::u8path(argv[3]);check(!std::filesystem::exists(directory),"Fixture output already exists");std::filesystem::create_directories(directory/"objects");
            std::ofstream manifest(directory/"scene-append.mes"),mappingFile(directory/"inventory.mes");unsigned i=900,j=0;
            for(const auto& obj:scene){auto bytes=encode_instance(obj);std::ofstream out(directory/"objects"/(obj.id+".mob"),std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());manifest<<"{"<<i++<<"}{"<<(obj.type==2?"chest":"object")<<" "<<obj.id<<"}\n";auto parent=inventory_parent(obj);if(!parent.empty())mappingFile<<"{"<<j++<<"}{"<<obj.id<<" "<<parent<<"}\n";}
            Bytes independent(48,0);auto put=[&](unsigned offset,std::uint32_t value){for(unsigned b=0;b<4;b++)independent[offset+b]=value>>(b*8);};put(8,1346*64+31);put(12,1024*64+28);put(16,123);put(20,std::uint32_t(-87));put(28,0x90000000);independent[32]=210;independent[33]=30;independent[34]=90;
            std::ofstream removed(directory/"removed-lights.bin",std::ios::binary);char one[4]={1,0,0,0};removed.write(one,4);removed.write(reinterpret_cast<const char*>(independent.data()),independent.size());
            std::ofstream nativeLights(directory/"test-native-lights.bin",std::ios::binary);nativeLights.write(reinterpret_cast<const char*>(independent.data()),independent.size());put(16,124);nativeLights.write(reinterpret_cast<const char*>(independent.data()),independent.size());put(16,123);independent[32]=101;nativeLights.write(reinterpret_cast<const char*>(independent.data()),independent.size());independent[32]=210;nativeLights.write(reinterpret_cast<const char*>(independent.data()),independent.size());
            std::cout<<"Native housing inventory fixture written\n";
        }
        if(argc==3){auto installation=std::make_shared<Installation>(std::filesystem::u8path(argv[1]));SceneAssets assets(installation);auto entries=assets.catalog();std::vector<GameObject> catalog;GameObject npc,weapon,coins;
            for(const auto& e:entries){catalog.push_back(e.object);if(e.object.type==16 && !npc.prototype_id)npc=clone_instance(e.object);if(e.object.type==5 && !weapon.prototype_id)weapon=clone_instance(e.object);if(e.object.type==8 && !coins.prototype_id)coins=clone_instance(e.object);}
            check(npc.prototype_id && weapon.prototype_id && coins.prototype_id,"Missing native inventory fixtures");set_number(npc,"LOCATION",32ull|(32ull<<32));set_number(weapon,"ITEM_INV_LOCATION",1004);set_number(coins,"ITEM_INV_LOCATION",0);set_number(coins,"GOLD_QUANTITY",200);
            auto native=inventory_apply({npc},npc.id,{weapon,coins},[&](const auto& o){return assets.effective(o);},size);
            InventoryEditor dialog(native,native[0],catalog,assets);dialog.show();app.processEvents();check(dialog.grab().save(QString::fromUtf8(argv[2])),"Inventory screenshot failed");
        }
        std::cout<<"PASS: native inventory references, equipment, preservation, roundtrip, removal, copy and validation\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
