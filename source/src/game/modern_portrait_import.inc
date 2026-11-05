/* Included by portrait.c. Publish images before their catalogue entry. Never
 * change an existing portrait ID: saved characters refer to those IDs. */
static bool portrait_import_matches(mes_file_handle_t candidate, int added, const char* name)
{
    int old_count=portrait_have_user_mes_file?mes_entries_count(portrait_user_mes_file):0;
    if(mes_entries_count(candidate)!=old_count+(added!=0))return false;
    for(int i=0;i<old_count;i++) {
        MesFileEntry old,check;
        if(!mes_get_entry(portrait_user_mes_file,i,&old))return false;
        check.num=old.num;
        if(!mes_search(candidate,&check) || strcmp(check.str,old.str))return false;
    }
    if(added) {
        MesFileEntry check={.num=added};
        if(!mes_search(candidate,&check) || strcmp(check.str,name))return false;
    }
    return true;
}

PortraitImportResult portrait_import(SDL_Surface* small, SDL_Surface* large, int* id)
{
    const char* catalogue="data/portrait/userport.mes";
    if(!small || !large || small->w!=64 || small->h!=64 || large->w!=128 || large->h!=128
        || small->format!=SDL_PIXELFORMAT_INDEX8 || large->format!=SDL_PIXELFORMAT_INDEX8)return PORTRAIT_IMPORT_IO;
    int count=portrait_have_user_mes_file?mes_entries_count(portrait_user_mes_file):0;
    if(count>=999)return PORTRAIT_IMPORT_FULL;
    /* Classic portrait navigation stops at a missing ID. Do not silently
     * renumber an existing, incompatible catalogue. */
    for(int n=1;n<=count;n++) {
        MesFileEntry entry={.num=n};
        if(!mes_search(portrait_user_mes_file,&entry))return PORTRAIT_IMPORT_CATALOGUE;
    }
    SDL_PathInfo info;
    bool local=SDL_GetPathInfo(catalogue,&info);
    char* original=NULL;size_t length=0;
    if(local)original=SDL_LoadFile(catalogue,&length);
    else if(portrait_have_user_mes_file) {
        TigFile* file=tig_file_fopen("portrait/userport.mes","rb");
        if(file) {
            int bytes=tig_file_filelength(file);
            if(bytes>=0 && bytes<=1024*1024) {
                length=(size_t)bytes;original=SDL_malloc(length+1);
                if(original && tig_file_fread(original,1,length,file)!=length){SDL_free(original);original=NULL;}
                if(original)original[length]=0;
            }
            tig_file_fclose(file);
        }
    } else original=SDL_strdup("");
    if(!original || length>1024*1024){SDL_free(original);return PORTRAIT_IMPORT_IO;}
    if(!SDL_CreateDirectory("data/portrait")){SDL_free(original);return PORTRAIT_IMPORT_IO;}

    static unsigned sequence;
    char name[80],normal[160],big[160],temp[160],backup[160];
    snprintf(name,sizeof(name),"ANYren%03d_%llu_%u",count+1,(unsigned long long)SDL_GetTicksNS(),++sequence);
    snprintf(normal,sizeof(normal),"data/portrait/%s.bmp",name);
    snprintf(big,sizeof(big),"data/portrait/%s_b.bmp",name);
    snprintf(temp,sizeof(temp),"data/portrait/%s.mes.tmp",name);
    snprintf(backup,sizeof(backup),"data/portrait/%s.mes.bak",name);
    if(SDL_GetPathInfo(normal,&info) || SDL_GetPathInfo(big,&info) || SDL_GetPathInfo(temp,&info)) {
        SDL_free(original);return PORTRAIT_IMPORT_IO;
    }
    PortraitImportResult result=PORTRAIT_IMPORT_IO;
    mes_file_handle_t staged=MES_FILE_HANDLE_INVALID;
    bool normal_written=false,big_written=false;
    SDL_IOStream* file=SDL_IOFromFile(temp,"wb");
    if(!file){SDL_free(original);return result;}
    bool ok=SDL_WriteIO(file,original,length)==length;
    if(!SDL_CloseIO(file))ok=false;
    /* Compare on-disk content with the currently loaded catalogue first. This
     * also detects external edits made while character creation was open. */
    if(!ok)goto cleanup;
    bool loaded=mes_load(temp,&staged);
    if((!loaded && count) || (loaded && !portrait_import_matches(staged,0,NULL))) {
        result=PORTRAIT_IMPORT_CATALOGUE;goto cleanup;
    }
    if(loaded)mes_unload(staged);
    staged=MES_FILE_HANDLE_INVALID;
    char record[128];int bytes=snprintf(record,sizeof(record),"\r\n{%d}{%s}\r\n",count+1,name);
    file=SDL_IOFromFile(temp,"ab");if(!file)goto cleanup;
    ok=SDL_WriteIO(file,record,bytes)==(size_t)bytes;
    if(!SDL_CloseIO(file))ok=false;
    if(!ok || !mes_load(temp,&staged))goto cleanup;
    if(!portrait_import_matches(staged,count+1,name)){result=PORTRAIT_IMPORT_CATALOGUE;goto cleanup;}
    /* Keep the original catalogue, including comments, as a recoverable backup. */
    if(local && !SDL_CopyFile(catalogue,backup))goto cleanup;
    normal_written=true;if(!SDL_SaveBMP(small,normal))goto cleanup;
    big_written=true;if(!SDL_SaveBMP(large,big))goto cleanup;
    if(!SDL_RenamePath(temp,catalogue))goto cleanup;
    if(portrait_have_user_mes_file)mes_unload(portrait_user_mes_file);
    portrait_user_mes_file=staged;portrait_have_user_mes_file=true;
    SDL_free(original);*id=count+1;return PORTRAIT_IMPORT_OK;
cleanup:
    if(staged!=MES_FILE_HANDLE_INVALID)mes_unload(staged);
    SDL_RemovePath(temp);
    if(normal_written)SDL_RemovePath(normal);
    if(big_written)SDL_RemovePath(big);
    SDL_free(original);return result;
}
