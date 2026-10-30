// Independent execution checks for the native instruction subset emitted by
// structured control flow. Operand decoding follows the engine's public format.
inline void checkAuthoring(){
    auto require=[](bool ok){if(!ok)throw std::runtime_error("Authoring regression failed");};
    auto execute=[&](const QString& source){auto compiled=worlded::compile_modern_script(source);const auto& b=compiled.bytes;std::map<std::pair<int,int>,int> vars;unsigned pc=0,steps=0;
        auto operand=[&](std::size_t p,int i){int type=b[p+4+i],value=std::int32_t(worlded::word(b,p+12+4*i));return type==3?value:vars[{type,value}];};
        while(pc<worlded::word(b,52)){require(++steps<1000);std::size_t p=64+pc*132;unsigned cond=worlded::word(b,p);bool yes=cond==0?true:cond==4?operand(p,0)==operand(p,1):cond==5?operand(p,0)<=operand(p,1):throw std::runtime_error("Unexpected condition");p+=yes?44:88;unsigned action=worlded::word(b,p);if(action==1 || action==2)break;if(action==3){pc=operand(p,0);continue;}if(action>=10 && action<=13){int value=operand(p,1);if(action==11)value+=operand(p,2);if(action==12)value-=operand(p,2);if(action==13)value*=operand(p,2);vars[{b[p+4],int(worlded::word(b,p+12))}]=value;}else require(action==0);++pc;}require(compiled.sourceLines.size()==int(worlded::word(b,52)));return vars[std::make_pair(1,1900)];};
    require(execute("let sum=0; for(let i=0;i<6;i++){if(i==2){continue;} if(i==5){break;} sum+=i;} global(1900)=sum;")==8);
    require(execute("let n=0; while(n<4){n++;} if(n>=4 && n<=4){global(1900)=7;}else{global(1900)=8;}")==7);
    require(execute("let n=3; if(n>3 || n!=3){global(1900)=9;}else if(!(n<3)){global(1900)=5;}")==5);
    require(execute("let n=0; for(let i=0;i<3;i++){for(let j=0;j<3;j++){if(j==1){break;} n++;}} global(1900)=n;")==3);
    require(execute("goto end; global(1900)=1; label end: global(1900)=-2147483648;")==INT32_MIN);
    require(execute("let x=2+3*4; global(1900)=x-1;")==13);
    for(auto bad:{"break;","let a=1;let a=2;","global(2000)=1;","flag(3200)=1;","if(true){","goto missing;","let x=1/0;","let x=-2147483649;"}){bool failed=false;try{worlded::compile_modern_script(bad);}catch(const std::exception&){failed=true;}require(failed);}
    {worlded::GameObject object;object.type=3;object.prototype_id=4000;object=worlded::clone_instance(object);worlded::set_number(object,"CURRENT_AID",0xe0000000);worlded::set_number(object,"AID",0xe0000000);worlded::set_number(object,"LOCATION",0);
        worlded::ObjectField field;field.name="OVERLAY_FORE";field.kind="UINT32_ARRAY";field.elements={{0,{0,0,0,0xe0}},{1,{0,0,0,0x40}}};object.fields.push_back(field);
        ActorEditor editor(object,object,"Flames");require(editor.effectBlends.size()==2);editor.effectBlends[0].box->setChecked(true);editor.effectBlends[1].box->setChecked(true);auto changed=editor.values();require(changed.number("CURRENT_AID")==0xe0000100 && changed.number("AID")==0xe0000100);require(worlded::word(changed.field("OVERLAY_FORE")->elements[0].data,0)==0xe0000100);require(worlded::word(changed.field("OVERLAY_FORE")->elements[1].data,0)==0x40000000);
        editor.tabs->setCurrentIndex(2);editor.show();QApplication::processEvents();editor.grab().save("/tmp/worlded-effect-blend.png");
    }
    {worlded::GameObject object;object.type=3;object.prototype_id=4046;object=worlded::clone_instance(object);worlded::set_number(object,"CURRENT_AID",1075316032);worlded::set_number(object,"AID",1075316032);worlded::set_number(object,"LOCATION",0);worlded::set_number(object,"BLIT_FLAGS",0x100);
        ActorEditor editor(object,object,"Lumière");require(editor.effectBlends.size()==1);editor.effectBlends[0].box->setChecked(true);auto changed=editor.values();require(changed.number("BLIT_FLAGS")==0x110);require(changed.number("CURRENT_AID")==object.number("CURRENT_AID"));ActorEditor reopen(changed,changed,"Lumière");require(reopen.effectBlends[0].box->isChecked());reopen.effectBlends[0].box->setChecked(false);require(reopen.values().number("BLIT_FLAGS")==0x100);
        editor.tabs->setCurrentIndex(2);editor.show();QApplication::processEvents();editor.grab().save("/tmp/worlded-light-effects.png");
    }
    {worlded::GameObject bed;bed.type=3;bed.prototype_id=4045;bed=worlded::clone_instance(bed);worlded::set_number(bed,"LOCATION",0);
        worlded::ObjectField field;field.name="SCRIPTS_IDX";field.kind="SCRIPT_ARRAY";field.present=false;worlded::Bytes data(12,0);data[8]=40001&255;data[9]=40001>>8;field.elements.push_back({1,data});bed.fields.push_back(field);
        ActorEditor editor(bed,bed,"Lit");require(editor.scriptTable->item(1,1)->text()=="40001");auto repaired=editor.values();require(repaired.field("SCRIPTS_IDX")->present);auto reopened=worlded::read_objects(worlded::encode_instance(repaired)).front();require(worlded::word(reopened.field("SCRIPTS_IDX")->elements.front().data,8)==40001);
    }
    auto lines=worlded::new_dialogue();lines[0].fields[1]="Bonjour, étranger !";auto bytes=worlded::encode_dialogue(lines);require(worlded::parse_dialogue(bytes)==lines);lines[1].fields[5]="99";bool failed=false;try{worlded::encode_dialogue(lines);}catch(const std::exception&){failed=true;}require(failed);
    worlded::DialogueEditor editor;int changed=0;editor.changed=[&]{++changed;};editor.load(worlded::new_dialogue());editor.list->setCurrentRow(1);require(changed==0);editor.add(false);require(changed==1);editor.list->setCurrentRow(0);editor.add(true);require(editor.value().size()==4);
}
