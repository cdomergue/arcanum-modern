static const char* journal_chapter_label(int tab)
{
    return tab==7?tr("Compagnons","Companions"):game_text(modern_logbook_tab_label(tab));
}
static void companion_journal_entry(int index,char* out,size_t size)
{
    const CompanionEntry* e=modern_companions_get(index);
    if(!e){*out=0;return;}
    char name[512]={0},place[512]={0};int area=0;
    from_game(description_get(e->description),name,sizeof(name));
    if(!*name)snprintf(name,sizeof(name),"%s",tr("Compagnon","Companion"));
    map_get_area(e->map,&area);
    if(!area && e->map==map_by_type(MAP_TYPE_START_MAP))area=area_get_nearest_area_in_range(e->location,false);
    const char* location_name=area>0?area_get_name(area):NULL;
    if(location_name)from_game(location_name,place,sizeof(place));
    else snprintf(place,sizeof(place),"%s",tr("Lieu non identifié","Unidentified location"));
    const char* status=tr("Situation inconnue","Unknown status");
    switch(e->state) {
    case COMPANION_PRESENT:status=tr("Dans le groupe","In your party");break;
    case COMPANION_WAITING:status=tr("En attente à cet endroit","Waiting at this location");break;
    case COMPANION_LEFT:status=tr("A quitté le groupe — motif non enregistré","Left the party — reason not recorded");break;
    case COMPANION_WAIT_EXPIRED:status=tr("A cessé d’attendre","Stopped waiting");break;
    case COMPANION_DEAD:status=tr("Décès constaté","Known to have died");break;
    case COMPANION_UNAVAILABLE:status=tr("N’est plus disponible à cet endroit","No longer available at this location");break;
    }
    char position[128];int west,south;
    if(modern_companions_coordinates(e->map,e->location,&west,&south))
        snprintf(position,sizeof(position),"%d %s, %d S",west,tr("O","W"),south);
    else snprintf(position,sizeof(position),"%s",tr("inconnues","unknown"));
    snprintf(out,size,"%s\n%s\n%s : %s\n%s : %s\n%s\n\n",name,status,
        tr("Dernier lieu connu","Last known location"),place,
        tr("Coordonnées","Coordinates"),position,
        e->state==COMPANION_PRESENT?"":e->recovered?tr("Retrouvé dans la sauvegarde ; date inconnue.","Recovered from the save; date unknown."):
            tr("Dernière observation ; sa position peut avoir changé.","Last observation; their position may have changed."));
}
