#pragma once
#include "modern_script.hpp"
#include "dialogue_editor.hpp"
#include <QtCore/QSaveFile>

namespace worlded {
inline void script_put(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    if(offset+4>bytes.size())throw std::runtime_error("Truncated script");
    for(int i=0;i<4;++i)bytes[offset+i]=std::uint8_t(value>>(8*i));
}
inline Bytes script_read_file(const QString& path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error(file.errorString().toStdString());
    auto data=file.readAll();return Bytes(data.begin(),data.end());
}
inline void script_write_file(const QString& path,const Bytes& bytes) {
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))throw std::runtime_error(file.errorString().toStdString());
    if(file.write(reinterpret_cast<const char*>(bytes.data()),qint64(bytes.size()))!=qint64(bytes.size()) || !file.commit())throw std::runtime_error(file.errorString().toStdString());
}
inline QString script_action_name(unsigned opcode,bool condition) {
    auto count=condition?std::size(conditionNames):std::size(actionNames);
    return opcode<count?QString::fromLatin1(condition?conditionNames[opcode]:actionNames[opcode]):QString("OPCODE_%1").arg(opcode);
}
struct ScriptProperties {
    QString description;
    std::uint32_t scriptFlags=0,localFlags=0,counters=0;
    static ScriptProperties read(const Bytes& bytes) {
        auto info=inspect_script(bytes);
        return {QString::fromStdString(cp1252_utf8(info.description)),word(bytes,48),word(bytes,0),word(bytes,4)};
    }
    Bytes apply(Bytes bytes)const {
        inspect_script(bytes);
        // Preserve legacy full-width descriptions and padding when unchanged.
        if(description!=read(bytes).description){auto encoded=utf8_cp1252(description.toStdString());if(encoded.size()>39 || description.contains(QChar(0)))throw std::runtime_error("Description must fit 39 Windows-1252 bytes and contain no NUL");std::fill(bytes.begin()+8,bytes.begin()+48,0);std::copy(encoded.begin(),encoded.end(),bytes.begin()+8);}
        script_put(bytes,48,scriptFlags);script_put(bytes,4,counters);script_put(bytes,0,localFlags);return bytes;
    }
};
// Native row edits retain all 132 bytes, including unused operands and padding.
// A relocation is explicit: ambiguous computed jumps and external references
// cannot be inferred from one SCR and must be reviewed by the author.
inline Bytes script_reorder(const Bytes& original,const std::vector<int>& order,bool relocate=true) {
    auto info=inspect_script(original);Bytes out(original.begin(),original.begin()+64);QMap<int,int> mapping;
    for(int i=0;i<int(order.size());++i){int old=order[i];if(old>=0){if(unsigned(old)>=info.lines || mapping.contains(old))throw std::runtime_error("Invalid row permutation");mapping[old]=i;out.insert(out.end(),original.begin()+64+132*old,original.begin()+64+132*(old+1));}else out.resize(out.size()+132,0);}
    script_put(out,52,unsigned(order.size()));script_put(out,56,std::max(info.capacity,unsigned(order.size())));
    if(relocate)for(std::size_t row=0;row<order.size();++row)for(int branch=1;branch<=2;++branch){auto p=64+row*132+branch*44;
        if(word(out,p)==3 && out[p+4]==3){int target=std::int32_t(word(out,p+12));if(!mapping.contains(target))throw std::runtime_error("A jump targets a removed instruction; change that jump before deleting the row");script_put(out,p+12,mapping[target]);}}
    inspect_script(out);return out;
}
inline QStringList script_warnings(const Bytes& bytes) {
    auto info=inspect_script(bytes);QStringList warnings;
    for(unsigned row=0;row<info.lines;++row)for(int part=0;part<3;++part){auto p=64+row*132+part*44;auto id=word(bytes,p);bool cond=part==0;
        if(id>=(cond?std::size(conditionNames):std::size(actionNames))){warnings<<QString("Instruction %1: unknown opcode %2 is preserved").arg(row).arg(id);continue;}
        if(!cond && id==3){if(bytes[p+4]!=3)warnings<<QString("Instruction %1 has a computed jump; review it after moving lines").arg(row);else if(word(bytes,p+12)>=info.lines)throw std::runtime_error(QString("Instruction %1 jumps outside this script").arg(row).toStdString());}
        if(!cond && (id==7 || id==54 || id==55))warnings<<QString("Instruction %1 calls another script by instruction number; review cross-script references").arg(row);
    }return warnings;
}
inline Bytes dialogue_source(const Dialogue& lines) {
    QJsonArray rows;for(const auto& row:lines){QJsonArray fields;for(const auto& field:row.fields)fields.append(field);rows.append(fields);}
    auto json=QJsonDocument(QJsonObject{{"format","renaissance-dialogue-source"},{"version",1},{"lines",rows}}).toJson();return Bytes(json.begin(),json.end());
}
inline Dialogue read_dialogue_source(const Bytes& bytes) {
    QJsonParseError error;auto doc=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(bytes.data()),int(bytes.size())),&error);auto obj=doc.object();
    if(error.error!=QJsonParseError::NoError || obj["format"]!="renaissance-dialogue-source" || obj["version"].toInt()!=1 || !obj["lines"].isArray())throw std::runtime_error("Invalid dialogue source");
    Dialogue result;for(auto value:obj["lines"].toArray()){auto fields=value.toArray();if(fields.size()!=7)throw std::runtime_error("Dialogue source needs seven fields per line");DialogueLine row;for(int i=0;i<7;++i){if(!fields[i].isString())throw std::runtime_error("Dialogue fields must be strings");row.fields[i]=fields[i].toString();}result.push_back(row);}return result;
}
inline Dialogue resolve_dialogue(Dialogue lines,const QMap<QString,int>& labels) {
    for(auto& row:lines){auto target=row.fields[5].trimmed();if(!target.startsWith('@'))continue;auto name=target.mid(1);
        if(!labels.contains(name))throw std::runtime_error(("Unknown script label: "+name).toStdString());
        if(labels[name]<=0)throw std::runtime_error("A dialogue cannot resume instruction zero; place this label after an initial instruction");
        row.fields[5]=QString::number(-labels[name]);}
    validate_dialogue(lines);return lines;
}
struct ScriptProjectBuild {QMap<QString,Bytes> files;QStringList warnings;};
inline ScriptProjectBuild compile_script_project(const QString& directory) {
    ScriptProjectBuild result;QMap<QString,ModernScriptResult> scripts;
    auto input=QDir(directory).canonicalPath();if(input.isEmpty())throw std::runtime_error("Project folder does not exist");
    QDirIterator sources(input,{"*.wscript"},QDir::Files,QDirIterator::Subdirectories);
    while(sources.hasNext()){
        auto path=sources.next(),stem=QFileInfo(path).completeBaseName();if(scripts.contains(stem.toCaseFolded()))throw std::runtime_error(("Duplicate script source: "+stem).toStdString());
        auto source=script_read_file(path);Bytes header=empty_script();QStringList candidates={QFileInfo(path).dir().filePath(stem+".scr"),input+"/exports/scr/"+stem+".scr"};for(auto candidate:candidates)if(QFileInfo::exists(candidate)){header=script_read_file(candidate);break;}
        auto script=compile_modern_script(QString::fromUtf8(reinterpret_cast<const char*>(source.data()),int(source.size())),header);scripts[stem.toCaseFolded()]=script;result.files["scr/"+stem+".scr"]=script.bytes;result.warnings+=script_warnings(script.bytes);
    }
    QDirIterator dialogues(input,{"*.wdlg"},QDir::Files,QDirIterator::Subdirectories);QSet<QString> seen;
    while(dialogues.hasNext()){auto path=dialogues.next(),stem=QFileInfo(path).completeBaseName();if(seen.contains(stem.toCaseFolded()))throw std::runtime_error(("Duplicate dialogue source: "+stem).toStdString());seen.insert(stem.toCaseFolded());auto source=read_dialogue_source(script_read_file(path));auto resolved=resolve_dialogue(source,scripts.value(stem.toCaseFolded()).labels);result.files["dlg/"+stem+".dlg"]=encode_dialogue(resolved);result.warnings+=validate_dialogue(resolved);}
    // Native-only resources remain useful in mixed projects. Source companions
    // take precedence over their compiled files, which may be out of date.
    QDirIterator natives(input,{"*.scr","*.dlg"},QDir::Files,QDirIterator::Subdirectories);QMap<QString,QString> nativePaths;
    while(natives.hasNext()){
        auto path=natives.next();auto info=QFileInfo(path);auto stem=info.completeBaseName().toCaseFolded();bool script=info.suffix().compare("scr",Qt::CaseInsensitive)==0;
        if(script?scripts.contains(stem):seen.contains(stem))continue;
        auto key=(script?"scr/":"dlg/")+info.fileName();auto folded=key.toCaseFolded();
        if(nativePaths.contains(folded))throw std::runtime_error(("Duplicate native resource: "+path+" and "+nativePaths[folded]).toStdString());
        nativePaths[folded]=path;
        auto bytes=script_read_file(path);if(script)result.warnings+=script_warnings(bytes);else result.warnings+=validate_dialogue(parse_dialogue(bytes));result.files[key]=bytes;
    }
    if(result.files.isEmpty())throw std::runtime_error("No script or dialogue found in the project");
    return result;
}
inline void export_script_project(const QString& directory,const ScriptProjectBuild& build) {
    QDir folder(directory);if(folder.exists() && !folder.entryList(QDir::NoDotAndDotDot|QDir::AllEntries|QDir::Hidden).isEmpty())throw std::runtime_error("Choose a new or empty export folder");
    if(!QDir().mkpath(directory))throw std::runtime_error("Cannot create export folder");
    for(auto it=build.files.begin();it!=build.files.end();++it){auto path=folder.filePath(it.key());if(!QDir().mkpath(QFileInfo(path).absolutePath()))throw std::runtime_error("Cannot create resource directory");script_write_file(path,it.value());}
}

}
