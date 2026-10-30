#pragma once
#include "worlded/assets.hpp"
#include <QtCore>
namespace worlded {
#include "script_labels.inc"
inline std::uint32_t word(const Bytes& data,std::size_t offset) {
    if(offset+4>data.size())throw std::runtime_error("Truncated script data");
    return data[offset]|(std::uint32_t(data[offset+1])<<8)|(std::uint32_t(data[offset+2])<<16)|(std::uint32_t(data[offset+3])<<24);
}
inline QString script_listing(const Bytes& data) {
    auto info=inspect_script(data);QString result=QString::fromStdString(cp1252_utf8(info.description))+QString("\n%1 instructions · read only\n\n").arg(info.lines);
    auto operation=[&](std::size_t offset,bool condition){auto id=word(data,offset);QString name;
        if(condition && id<std::size(conditionNames))name=conditionNames[id];else if(!condition && id<std::size(actionNames))name=actionNames[id];else name=QString("Opcode %1").arg(id);
        // Unused operand slots contain arbitrary bytes in original SCR files.
        unsigned mask=condition?(id<std::size(conditionOperands)?conditionOperands[id]:255):(id<std::size(actionOperands)?actionOperands[id]:255);
        QStringList args;for(unsigned n=0;n<8;n++)if(mask&(1u<<n))args<<QString("arg%1 [type %2] = %3").arg(n).arg(data[offset+4+n]).arg(std::int32_t(word(data,offset+12+4*n)));
        return name+" ("+args.join(", ")+")";};
    for(unsigned i=0;i<info.lines;i++){auto p=64+i*132;result+=QString("%1  IF %2\n    THEN %3\n    ELSE %4\n\n").arg(i).arg(operation(p,true),operation(p+44,false),operation(p+88,false));}
    return result;
}
}
