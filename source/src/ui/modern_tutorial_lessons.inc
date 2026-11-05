#include "game/rumor.h"
#include "game/mp_utils.h"

/* Observe the same character, panel and combat state that drives the HUD. */
static int64_t tutorial_actor(const char* guid)
{
    ObjectID id;
    return objid_id_from_str(&id,guid)?obj_pool_perm_lookup(id):OBJ_HANDLE_NULL;
}

static bool tutorial_wolves_defeated(void)
{
    static const char* const ids[]={
        "G_3088ABCB_B0F1_324F_8474_BE7A8195CE9D",
        "G_FD902837_6278_2F49_AA8A_E397622F6BD3",
        "G_886E4ED5_6090_7D4D_AA2A_E26D8F582DC4"
    };
    for (size_t i=0;i<SDL_arraysize(ids);i++) {
        int64_t wolf=tutorial_actor(ids[i]);
        if (!wolf || !critter_is_dead(wolf)) return false;
    }
    return true;
}

static unsigned tutorial_bound_slot(void)
{
    unsigned result=0;
    for (int i=0;i<20;i++) {
        Hotkey* slot=modern_hotkey_get(i);
        if(slot)result=result*31u+(unsigned)slot->type*17u+(unsigned)slot->data+(unsigned)slot->item_obj.obj;
    }
    return result;
}

static void tutorial_draw_lesson(void)
{
    static int previous=-1;
    static int64_t last_pc,initial_guide_loc,initial_weapon,initial_armor;
    static int initial_count;
    static unsigned initial_slot;
    static float initial_zoom;
    static bool initial_toggle;
    tutorial_next_rect=(SDL_FRect){0};
    if(!tutorial_available || tutorial_starting || !visible())return;
    int64_t pc=player_get_local_pc_obj();
    int64_t guide=tutorial_actor("G_9446D492_810B_D640_A404_25CBFAC90F01");
    if(!pc || !guide){tutorial_voice_stop();return;}
    int saved=script_local_counter_get(guide,SAP_DIALOG,0)&255;
    if(tutorial_new_session){
        tutorial_new_session=false;previous=-1;
        script_local_counter_set(guide,SAP_DIALOG,0,TUTORIAL_HELLO);
        saved=TUTORIAL_HELLO;
    }
    if(saved<TUTORIAL_GUIDE || saved>TUTORIAL_HELLO)return;
    tutorial_departure=false;tutorial_step=(enum TutorialStep)saved;
    int64_t loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
    int64_t weapon=item_wield_get(pc,ITEM_INV_LOC_WEAPON),armor=item_wield_get(pc,ITEM_INV_LOC_ARMOR);
    int panel=native_panel();
    // Persistent gift flags also repair sessions started before supplies existed.
    int gifts=script_local_counter_get(guide,SAP_DIALOG,1);
    if(critter_leader_get(guide)==pc){
        if(!(gifts&1)){
            int64_t model=item_wield_get(guide,ITEM_INV_LOC_WEAPON),gift=0;
            if(model)obj_create_inst(obj_field_handle_get(model,OBJ_F_PROTOTYPE_HANDLE),loc,&gift);
            if(gift){item_transfer(gift,pc);gifts|=1;}
        }
        if(!(gifts&2)){
            int64_t gift=item_gold_create(20,loc);
            if(gift){item_transfer(gift,pc);gifts|=2;}
        }
        if(!(gifts&4)){
            int64_t gift=0;
            if(mp_object_create(11079,loc,&gift) && gift){item_transfer(gift,pc);gifts|=4;}
        }
        if(!(gifts&16)){
            int64_t gift=0;
            if(mp_object_create(10134,loc,&gift) && gift){item_transfer(gift,pc);gifts|=16;}
        }
        if(!(gifts&8)){
            const int states[]={QUEST_STATE_ACCEPTED,QUEST_STATE_COMPLETED,QUEST_STATE_BOTCHED,QUEST_STATE_MENTIONED};
            for(int i=0;i<4;i++){
                char error[128];
                int before=states[i]==QUEST_STATE_MENTIONED?QUEST_STATE_MENTIONED:QUEST_STATE_ACCEPTED;
                quest_state_set(pc,1980+i,before,guide);
                quest_state_migrate(pc,1980+i,before,states[i],false,error,sizeof(error));
            }
            for(int i=0;i<4;i++)rumor_known_set(pc,2980+i);
            gifts|=8;dirty=true;
        }
        script_local_counter_set(guide,SAP_DIALOG,1,gifts);
    }
    if(previous!=saved || last_pc!=pc){
        previous=saved;last_pc=pc;tutorial_origin=loc;
        tutorial_voice_stop();
        if(tutorial_step==TUTORIAL_HELLO)tutorial_hello_started=SDL_GetTicks();
        if(tutorial_step==TUTORIAL_ROUTE)tutorial_route_started=false;
        if(tutorial_step==TUTORIAL_SERVICES)tutorial_pause_seen=false;
        initial_guide_loc=obj_field_int64_get(guide,OBJ_F_LOCATION);
        initial_weapon=weapon;initial_armor=armor;initial_slot=tutorial_bound_slot();
        initial_zoom=tutorial_step==TUTORIAL_CAMERA?world_zoom:map_camera.scale;
        initial_count=tutorial_step==TUTORIAL_TRANSFER?item_gold_get(guide):tutorial_step==TUTORIAL_NOTE?map_memo_count:tutorial_step==TUTORIAL_TURN?combat_turn_based_turn_get():quest_filter;
        initial_toggle=tutorial_step==TUTORIAL_COMPARE?inventory_compare:tutorial_step==TUTORIAL_AI?combat_companion_ai_get():journal_favorites;
        bool locked=(tutorial_step==TUTORIAL_HELLO || tutorial_step<TUTORIAL_FIGHT) && !(tutorial_step==TUTORIAL_ROUTE && script_local_counter_get(guide,SAP_DIALOG,2));
        // The map has open wall segments as well as portal objects. Seal the
        // complete exterior ring so diagonal routes cannot bypass a doorway.
        for(int y=25;y<=35;y++)for(int x=17;x<=27;x++){
            if(x!=17 && x!=27 && y!=25 && y!=35)continue;
            int64_t edge=location_make(400*64+x,786*64+y);
            if(tileblock_is_tile_blocked(edge)!=locked)tileblock_set_tile_blocked(edge,locked);
        }
        if(obj_field_int32_get(guide,OBJ_F_CRITTER_PORTRAIT)!=1005)
            obj_field_int32_set(guide,OBJ_F_CRITTER_PORTRAIT,1005);
    }
    if(panel==4){tutorial_voice_stop();return;}
    int target=-1;
    bool complete=false;
    const char* line=NULL;
    const char* voice=NULL;
    switch(tutorial_step){
    case TUTORIAL_HELLO:
        voice="TUTORIAL_HELLO";
        line=tr("Bonjour et bienvenue dans Arcanum Renaissance. Ce tutoriel vous fera découvrir ce qui rend ce mod unique.",
                "Hello, and welcome to Arcanum Renaissance. In this tutorial, you will discover what makes this mod unique.");
        complete=SDL_GetTicks()-tutorial_hello_started>=9000;
        break;
    case TUTORIAL_GUIDE:
        voice="TUTORIAL_GUIDE";
        line=tr("Parlez au garde dans la maison et invitez-le à rejoindre le groupe.",
                "Talk to the guard in the house and ask him to join you.");
        target=-1; complete=guide && critter_leader_get(guide)==pc;
        break;
    case TUTORIAL_MOVE:
        voice="TUTORIAL_MOVE";
        line=tr("Le portrait du garde affiche sa vie et sa fatigue. Cliquez sur le plancher pour marcher ensemble.",
                "The guard’s portrait shows his health and fatigue. Click the floor to walk together.");
        target=1800; complete=loc!=tutorial_origin;
        break;
    case TUTORIAL_CAMERA:
        voice="TUTORIAL_CAMERA";
        line=tr("Utilisez la molette pour zoomer. Dans les options graphiques, la caméra peut suivre le personnage ou rester libre.",
                "Use the wheel to zoom. Graphics options let the camera follow your character or move freely.");
        target=-1; complete=world_zoom!=initial_zoom;
        break;
    case TUTORIAL_BAG:
        voice="TUTORIAL_BAG";
        line=tr("Ouvrez votre inventaire. Les objets portés apparaissent à gauche et le sac au centre.",
                "Open your inventory. Worn equipment appears on the left and the pack in the middle.");
        target=2; complete=panel==2;
        break;
    case TUTORIAL_SEARCH:
        voice="TUTORIAL_SEARCH";
        line=tr("Cliquez sur la recherche et tapez quelques lettres. Effacez-les pour retrouver tout votre sac.",
                "Click the search field and type a few letters. Clear it to see your whole pack again.");
        target=panel==2?2400:2; complete=panel==2 && *inventory_query;
        break;
    case TUTORIAL_FILTER:
        voice="TUTORIAL_FILTER";
        line=tr("Choisissez une catégorie, par exemple Armes. Tous réaffiche chaque catégorie.",
                "Choose a category such as Weapons. All shows every category again.");
        target=panel==2?2402:2; complete=panel==2 && inventory_filter!=0;
        break;
    case TUTORIAL_DETAILS:
        voice="TUTORIAL_DETAILS";
        line=tr("Revenez à Tous, puis survolez un objet. Lisez sa fiche à droite ; faites défiler les détails longs.",
                "Return to All, then hover over an item. Read its card on the right and scroll through longer descriptions.");
        target=panel==2?-1:2; complete=panel==2 && inventory_detail_object!=0;
        break;
    case TUTORIAL_COMPARE:
        voice=inventory_compare?"TUTORIAL_COMPARE_1":"TUTORIAL_COMPARE_2";
        line=inventory_compare?
            tr("La comparaison est activée. Survolez l’épée donnée par le garde : sa fiche apparaît avec celle de votre arme équipée.",
               "Comparison is enabled. Hover over the guard’s sword to see its details alongside your equipped weapon."):
            tr("Activez Comparer l’équipement, puis survolez l’épée donnée par le garde.",
               "Enable Compare equipment, then hover over the sword given by the guard.");
        target=panel==2?(inventory_compare?-1:2406):2; complete=panel==2 && inventory_compare && inventory_compare_rect.h>0;
        break;
    case TUTORIAL_EQUIP:
        voice="TUTORIAL_EQUIP";
        line=tr("Équipez ou retirez une arme ou une armure. Vous pouvez ensuite remettre votre équipement préféré.",
                "Equip or remove a weapon or armour. You can then restore your preferred equipment.");
        target=panel==2?-1:2; complete=panel==2 && (weapon!=initial_weapon || armor!=initial_armor);
        break;
    case TUTORIAL_TRANSFER:
        voice="TUTORIAL_TRANSFER";
        line=tr("Le garde vous a donné 20 pièces. Ouvrez l’échange avec lui et donnez-lui une partie de votre or. Maj + clic transfère toute la pile.",
                "The guard gave you 20 coins. Open trade with him and give him some of your gold. Shift-click transfers the whole stack.");
        target=-1; complete=item_gold_get(guide)>initial_count;
        break;
    case TUTORIAL_HOTBAR: {
        voice="TUTORIAL_HOTBAR";
        line=tr("Dans l’inventaire d’Erwin, faites glisser la potion donnée par le garde vers la case 1 de la barre rapide.",
                "In Merwin’s inventory, drag the potion given by the guard to quick slot 1.");
        Hotkey* slot=modern_hotkey_get(0);
        target=panel==2?300:2;
        complete=slot && slot->type==HOTKEY_ITEM && slot->item_obj.obj
            && obj_handle_is_valid(slot->item_obj.obj)
            && obj_field_int32_get(slot->item_obj.obj,OBJ_F_TYPE)==OBJ_TYPE_FOOD;
        break;
    }
    case TUTORIAL_BANK:
        voice="TUTORIAL_BANK";
        line=tr("Fermez le panneau et maintenez Maj pour afficher la deuxième banque de dix raccourcis.",
                "Close the panel and hold Shift to see the second bank of ten quick slots.");
        target=300+hotbar_offset(); complete=hotbar_offset()==10;
        break;
    case TUTORIAL_DETACH:
        voice="TUTORIAL_DETACH";
        line=tr("Sur le portrait du garde, cliquez sur le petit bouton de liaison pour le détacher du groupe.",
                "Click the small link button on the guard’s portrait to detach him from the party.");
        target=2600; complete=modern_party_detached(guide);
        break;
    case TUTORIAL_POSITION:
        voice="TUTORIAL_POSITION";
        line=tr("Sélectionnez le garde détaché et envoyez-le près de la cheminée.",
                "Select the detached guard and move him near the fireplace.");
        target=1800; complete=obj_field_int64_get(guide,OBJ_F_LOCATION)!=initial_guide_loc;
        break;
    case TUTORIAL_REGROUP:
        voice="TUTORIAL_REGROUP";
        line=tr("Utilisez le bouton de liaison pour que le garde recommence à vous suivre.",
                "Use the link button to have the guard follow you again.");
        target=2600; complete=!modern_party_detached(guide);
        break;
    case TUTORIAL_MAP:
        voice="TUTORIAL_MAP";
        line=tr("Ouvrez la carte locale de l’île. Les zones encore inexplorées restent masquées.",
                "Open the island’s local map. Unexplored areas remain covered.");
        target=3; complete=panel==3;
        break;
    case TUTORIAL_ZOOM:
        voice="TUTORIAL_ZOOM";
        line=tr("Zoomez sur la maison avec + ou la molette. Faites glisser la carte pour voir l’autre partie de l’île.",
                "Zoom in on the house with + or the wheel. Drag the map to see the other part of the island.");
        target=panel==3?750:3; complete=panel==3 && map_camera.scale!=initial_zoom;
        break;
    case TUTORIAL_NOTE:
        voice="TUTORIAL_NOTE";
        line=tr("Choisissez Ajouter une note, cliquez près de la maison, écrivez un repère et enregistrez-le.",
                "Choose Add note, click near the house, enter a landmark and save it.");
        target=panel==3?2190:3; complete=panel==3 && map_memo_count>initial_count && memo_edit<0;
        break;
    case TUTORIAL_ROUTE:
        voice="TUTORIAL_ROUTE";
        line=tr("Les sorties sont ouvertes. Tracez un chemin vers la maison à l’est sur la carte, puis cliquez sur « Suivre le chemin tracé ».",
                "The exits are open. Plot a route to the house to the east on the map, then click Follow the plotted route.");
        target=panel==3?(map_local.count>0?743:-1):3; complete=tutorial_route_started && loc!=tutorial_origin;
        break;
    case TUTORIAL_JOURNAL:
        voice="TUTORIAL_JOURNAL";
        line=tr("Fermez la carte et ouvrez le journal. Ses onglets séparent quêtes, rumeurs et autres découvertes.",
                "Close the map and open the journal. Its tabs separate quests, rumours and other discoveries.");
        target=0; complete=journal;
        break;
    case TUTORIAL_JOURNAL_FILTER:
        voice="TUTORIAL_JOURNAL_FILTER";
        line=tr("Essayez la recherche, les favoris ou un filtre de quêtes pour réduire la liste du journal.",
                "Try search, favourites or a quest filter to narrow the journal list.");
        target=journal?2300:0; complete=journal && (quest_filter!=initial_count || *search || journal_favorites!=initial_toggle);
        break;
    case TUTORIAL_TRACK:
        voice="TUTORIAL_TRACK";
        line=tr("Marquez une quête avec l’étoile, essayez Favoris, puis épinglez la quête des loups pour afficher son suivi sur le HUD.",
                "Star a quest, try Favourites, then pin the wolf quest to track it on the HUD.");
        target=-1; complete=tracked_quest==1980 && journal_mark_count>0;
        break;
    case TUTORIAL_SHEET:
        voice="TUTORIAL_SHEET";
        line=tr("Ouvrez la fiche du personnage. Consultez ses caractéristiques, ses compétences et les points à dépenser à la montée de niveau.",
                "Open the character sheet to review attributes, skills and points available when levelling up.");
        target=1; complete=panel==1;
        break;
    case TUTORIAL_SPELLS: {
        voice="TUTORIAL_SPELLS";
        line=tr("Ouvrez Magie. Faites glisser le sort du parchemin d’Agilité du Feu vers la case 2 de la barre rapide. Le parchemin sera consommé au lancement.",
                "Open Magic. Drag the spell from the Agility of Fire scroll to quick slot 2. Casting it consumes the scroll.");
        Hotkey* slot=modern_hotkey_get(1);
        target=panel==5 && selector==1?301:6;
        complete=slot && slot->type==HOTKEY_ITEM_SPELL && slot->item_obj.obj
            && obj_handle_is_valid(slot->item_obj.obj)
            && obj_field_int32_get(slot->item_obj.obj,OBJ_F_TYPE)==OBJ_TYPE_SCROLL;
        break;
    }
    case TUTORIAL_SKILLS: {
        voice="TUTORIAL_SKILLS";
        line=tr("Ouvrez Compétences. Faites glisser Vol à la tire vers la case 3 de la barre rapide.",
                "Open Skills. Drag Pick Pocket to quick slot 3.");
        Hotkey* slot=modern_hotkey_get(2);
        target=panel==5 && selector==2?302:5;
        // Skill hotkeys store the four-button selector index, not the Skill enum.
        complete=slot && slot->type==HOTKEY_SKILL && slot->data==1;
        break;
    }
    case TUTORIAL_SCHEMATICS:
        voice="TUTORIAL_SCHEMATICS";
        line=tr("Ouvrez les schémas pour consulter les composants de fabrication.",
                "Open the schematics panel to inspect crafting components.");
        target=8; complete=panel==7;
        break;
    case TUTORIAL_REST:
        voice="TUTORIAL_REST";
        line=tr("Ouvrez le repos pour examiner les durées proposées, puis refermez le panneau.",
                "Open the rest panel to review the available durations, then close it.");
        target=9; complete=panel==6;
        break;
    case TUTORIAL_SERVICES:
        voice="TUTORIAL_SERVICES";
        line=tr("Appuyez sur Échap pour ouvrir le menu des sauvegardes et des options, puis revenez au jeu. Cette étape se valide à votre retour.",
                "Press Escape to open the save and options menu, then return to the game. This step completes when you return.");
        target=-1; complete=tutorial_pause_seen;
        break;
    case TUTORIAL_FIGHT:
        voice="TUTORIAL_FIGHT";
        line=tr("Mission du garde : les sorties sont libres. Traversez vers l’est et combattez les trois loups en tour par tour.",
                "Guard’s mission: the exits are open. Cross to the east and fight the three wolves in turn-based combat.");
        target=7; complete=tactical_active();
        break;
    case TUTORIAL_TURN:
        voice="TUTORIAL_TURN";
        line=tr("L’initiative détermine l’ordre en haut. Survolez une cible pour prévisualiser le coût en PA, attaquez puis terminez le tour.",
                "Initiative sets the order at the top. Hover over a target to preview the AP cost, attack, then end the turn.");
        target=1100; complete=combat_turn_based_turn_get()!=initial_count;
        break;
    case TUTORIAL_AI:
        voice="TUTORIAL_AI";
        line=tr("Basculez le contrôle du compagnon. En manuel, deux alliés dont les tours se suivent peuvent se partager le tour.",
                "Toggle companion control. In manual mode, allies with consecutive turns can share their turn.");
        target=1109; complete=combat_companion_ai_get()!=initial_toggle;
        break;
    case TUTORIAL_LOG:
        voice="TUTORIAL_LOG";
        line=tr("Consultez le journal de combat pour lire les attaques et leurs résultats. Battez les trois loups.",
                "Open the combat log to read attacks and outcomes. Defeat all three wolves.");
        target=1106; complete=false;
        break;
    case TUTORIAL_DONE:
        voice="TUTORIAL_DONE";
        line=tr("Les deux secteurs sont sûrs. Vous pouvez explorer, ou revenir au menu pour rejouer le tutoriel.",
                "Both sectors are safe. Explore the island or return to the menu to replay the tutorial.");
        target=-1; complete=false;
        break;
    default:return;
    }
    bool optional=tutorial_step>TUTORIAL_GUIDE && tutorial_step<TUTORIAL_FIGHT;
    bool defeated=tutorial_step>=TUTORIAL_FIGHT && tutorial_step<TUTORIAL_DONE && tutorial_wolves_defeated();
    if(defeated || complete || (optional && tutorial_skip)){
        int next=defeated?TUTORIAL_DONE:tutorial_step+1;
        if(tutorial_step==TUTORIAL_HELLO)next=TUTORIAL_GUIDE;
        tutorial_voice_stop();
        if(tutorial_step==TUTORIAL_NOTE)next=TUTORIAL_JOURNAL;
        if(tutorial_step==TUTORIAL_SERVICES){
            script_local_counter_set(guide,SAP_DIALOG,2,1);next=TUTORIAL_ROUTE;
        }
        if(tutorial_step==TUTORIAL_ROUTE)next=script_local_counter_get(guide,SAP_DIALOG,2)?TUTORIAL_FIGHT:TUTORIAL_JOURNAL;
        script_local_counter_set(guide,SAP_DIALOG,0,next);
        if(defeated)quest_state_set(pc,1980,QUEST_STATE_COMPLETED,guide);
        if(defeated)tutorial_save_marker("completed-v1");
        tutorial_skip=false;
        return;
    }
    tutorial_skip=false;
    if(voice)tutorial_voice_play(voice);
    for(int i=0;i<button_count;i++)if(buttons[i].action==target && buttons[i].enabled){
        SDL_FRect r=buttons[i].rect;
        float pulse=0.5f+0.5f*sinf((float)(SDL_GetTicks()%1600)*6.2831853f/1600.0f);
        SDL_Color glow=GOLD;glow.a=(Uint8)(100+155*pulse);
        float expand=1.0f+2.0f*pulse;
        r.x-=expand;r.y-=expand;r.w+=expand*2;r.h+=expand*2;
        for(int k=0;k<3;k++){outline(r,glow);r.x--;r.y--;r.w+=2;r.h+=2;}
        break;
    }
    float width=fminf(1050,canvas_w-32);
    char hint[768];int binding=target>=0?key_for_action(target):-1;
    if(binding>=0)snprintf(hint,sizeof(hint),"%s  [%s]",line,key_name(binding));
    else SDL_strlcpy(hint,line,sizeof(hint));
    float text_width=width-(optional?145:24);
    int rows=menu_paragraph(hint,(SDL_FRect){0,0,text_width,0},0,GOLD);
    float height=fmaxf(60.0f,rows*text_line_height()+16.0f);
    SDL_FRect note={(canvas_w-width)/2,panel||journal?canvas_h-height-5:112,width,height};
    fill(note,(SDL_Color){28,21,15,245});outline(note,GOLD);
    menu_paragraph(hint,(SDL_FRect){note.x+12,note.y+8,text_width,rows*text_line_height()},0,GOLD);
    if(optional){
        tutorial_next_rect=(SDL_FRect){note.x+note.w-117,note.y+9,106,42};
        menu_plaque(tutorial_next_rect,true,false);
        button_label(1,tr("Passer","Skip"),tutorial_next_rect,true,GOLD);
    }
}