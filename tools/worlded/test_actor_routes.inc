// Included beside the NPC fixture in test_explorer.cpp (instance, resolved).
{
    const std::uint64_t routeSector=970ull|(1027ull<<26);
    ActorEditor routine(instance,resolved,"Routine fixture",nullptr,nullptr,routeSector);
    check(encode_instance(routine.values())==encode_instance(instance),"Untouched NPC routine changed inherited fields");
    routine.findChild<QLineEdit*>("NPC_STANDPOINT_DAY_X")->setText(QString::number(970*64+12));
    routine.findChild<QLineEdit*>("NPC_STANDPOINT_DAY_Y")->setText(QString::number(1027*64+15));
    routine.appendWaypoint(std::uint64_t(970*64+8)|(std::uint64_t(1027*64+9)<<32));
    routine.appendWaypoint(std::uint64_t(971*64+2)|(std::uint64_t(1027*64+3)<<32));
    auto modified=routine.values();
    check(modified.number("NPC_STANDPOINT_DAY")== (12ull|(15ull<<32)),"Day position did not localize to sector");
    check(modified.number("NPC_STANDPOINT_NIGHT")==instance.number("NPC_STANDPOINT_NIGHT"),"Day edit changed night position");
    check(modified.field("NPC_WAYPOINTS_IDX")->elements.size()==2,"Waypoints were not serialized");
    auto local=encode_instance(modified);transform_npc_routes(modified,routeSector,true);
    check(modified.number("NPC_STANDPOINT_DAY")== (std::uint64_t(970*64+12)|(std::uint64_t(1027*64+15)<<32)),"World route conversion failed");
    const auto& external=modified.field("NPC_WAYPOINTS_IDX")->elements[1].data;
    check(word(external,0)==971*64+2 && word(external,4)==1027*64+3,"External waypoint was moved to current sector");
    transform_npc_routes(modified,routeSector,false);check(encode_instance(modified)==local,"NPC routes failed global/local roundtrip");
    routine.routeTable->item(0,0)->setText("-1");bool badRoute=false;try{routine.values();}catch(...){badRoute=true;}check(badRoute,"Negative waypoint accepted");
    routine.routeTable->item(0,0)->setText("12");routine.routeTable->item(0,1)->setText("15");bool ambiguous=false;try{routine.values();}catch(...){ambiguous=true;}check(ambiguous,"Ambiguous external sector-zero route accepted");

    ActorEditor generator(instance,resolved,"Generator fixture");
    generator.generatorEnabled->setChecked(true);generator.generatorId->setValue(231);generator.generatorConcurrent->setValue(17);generator.generatorTotal->setValue(93);generator.generatorRate->setCurrentIndex(4);generator.generatorFlags[0]->setChecked(true);generator.generatorFlags[4]->setChecked(true);
    auto spawned=generator.values();auto data=spawned.number("NPC_GENERATOR_DATA");
    check(((data>>19)&255)==231 && ((data>>14)&31)==17 && ((data>>7)&127)==93 && (data&127)==(resolved.number("NPC_GENERATOR_DATA")&127),"Generator limits/counters encoded incorrectly");
    check((spawned.number("NPC_FLAGS")&0xe80000)==0x880000 && (spawned.number("FLAGS")&0x400002)==0x400002,"Generator flags/rate encoded incorrectly");
    ActorEditor knowledge(instance,resolved,"Knowledge fixture");knowledge.findChild<QSpinBox*>("knowledge_0")->setValue(5);knowledge.findChild<QSpinBox*>("knowledge_17")->setValue(7);
    auto learned=knowledge.values();unsigned college=0,technology=0;for(auto& e:learned.field("CRITTER_SPELL_TECH_IDX")->elements){if(e.index==0)college=word(e.data,0);if(e.index==17)technology=word(e.data,0);}check(college==5 && technology==7,"College/technology levels encoded incorrectly");
    ActorEditor scripts(instance,resolved,"Script attachment fixture");scripts.scriptTable->item(0,1)->setText("42");scripts.scriptTable->item(0,2)->setText("0x80000001");for(int i=0;i<4;i++)scripts.scriptTable->item(0,3+i)->setText(QString::number(i==3?255:i+1));
    auto scripted=scripts.values();const auto& record=scripted.field("SCRIPTS_IDX")->elements.front().data;
    check(word(record,0)==0x80000001 && word(record,4)==0xff030201 && word(record,8)==42,"Object attachment flags/counters encoded incorrectly");
    scripts.scriptTable->item(0,1)->setText("0");auto disabled=scripts.values();const auto& stopped=disabled.field("SCRIPTS_IDX")->elements.front().data;check(word(stopped,8)==0 && word(stopped,4)==0xff030201,"Disabled attachment lost its counters");
    scripts.scriptTable->item(0,3)->setText("256");bool badCounter=false;try{scripts.values();}catch(...){badCounter=true;}check(badCounter,"Out-of-range script counter accepted");
}
