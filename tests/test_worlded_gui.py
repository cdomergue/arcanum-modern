#!/usr/bin/env python3
"""Run native Qt interactions with synthetic resources, on all three platforms."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import zlib
from test_worlded import archive, art_file, game_object, sector, u32, building_prototypes, prototype, script

with tempfile.TemporaryDirectory(prefix='worlded-gui-') as temporary:
    root = Path(temporary)
    data = bytearray(sector(objects=False)[:-4] + game_object(True) + u32(1))
    data[52:52+4096*4] = bytes(4096*4)
    resources = [
        ('art/sprite.art', art_file(rotations=8), True),
        ('art/scenery/sprite.art', art_file(), True),
        ('art/scenery/scenery.mes', b'{1}{sprite.art}{22001}{unused-bed.art}', False),
        ('art/scenery/unused-bed.art', art_file(), False),
        ('art/tile/tstbse0a.art', art_file(frames=1), False),
        ('art/tile/tilename.mes', b'{300}{tst}', False),
        ('art/bad.art', b'broken', False),
        ('art/wall/structure.mes', b'{0}{int ext}', False),
        ('art/wall/intbseU0.art', art_file(frames=1), False),
        ('art/wall/extbseU0.art', art_file(frames=1), False),
        ('art/portal/portal.mes', b'{0}{door.art 2028}{13}{door.art 2028}{1001}{window.art 2036}{1002}{paneu0.art 2036}', False),
        ('art/portal/door.art', art_file(rotations=8, frames=1), False),
        ('art/portal/window.art', art_file(rotations=8, frames=1), False),
        ('art/portal/paneu0.art', art_file(rotations=8, frames=1), False),
        ('art/portal/paned0.art', art_file(rotations=8, frames=1), False),
        ('maps/test/0.sec', data, True),
        ('mes/notes.mes', 'Commentaire\n{1}{Propriété à Tarante}\n'.encode('cp1252'), False),
    ]
    resources += building_prototypes()
    # Facade frames share no rotation/palette bits with ordinary object AIDs.
    # Mark a high frame so selecting frame zero cannot accidentally pass.
    facade=bytearray(art_file(rotations=8,frames=1024,rle=False))
    facade[132+2048+8*1024*28+715*8]=7
    resources += [('art/facade/facadename.mes',b'{7}{rug-test}{263}{rug-high-test}',False),
                  ('art/facade/rug-test.art',facade,False),
                  ('art/facade/rug-high-test.art',facade,False)]
    resources += [
        ('art/roof/roofname.mes',b'{0}{roof}',False),
        ('art/roof/roof.art',art_file(frames=9),False),
        ('art/critter/hmm/hmmuwxaa.art',art_file(rotations=8,frames=1),False),
        ('scr/00042example.scr',script(),False),
        ('sound/schemeindex.mes',b'{100}{M.Wilderness #100}{4100}{A.Moors #4100}',False),
        ('rules/lighting schemes.mes',b'{7}{daylight.mes}',False),
        ('art/tile/drtgrs6a.art',art_file(frames=1),False),
    ]
    # This prototype is absent from every sector and shares another prototype's
    # AID. A global catalogue must still expose both as distinct entries.
    resources.append(('proto/003045.pro',prototype(3045,2),False))
    resources.append(('proto/001021.pro',prototype(1021,0),False))
    resources.append(('proto/004026.pro',prototype(4026,3),False))
    # A prototype with two inherited script attachments, to check that removing
    # one instance attachment does not erase the other or modify the prototype.
    inherited=prototype(4027,3)
    nullable={1,5,6,7,15,16,17,30,31,34,35}
    offset=68+sum(1 if i in nullable else 4 for i in range(31))
    scripts=b'\1'+struct.pack('<3I',12,2,0)+struct.pack('<6I',0,0,42,0,0,43)+u32(1)+u32(1|(1<<9))
    resources.append(('proto/004027.pro',inherited[:offset]+scripts+inherited[offset+1:],False))
    def npc_prototype(number, aid):
        # Independent NPC fixture: BASE, CRITTER and NPC field groups.
        base=prototype(4026,3); nullable={1,5,6,7,15,16,17,30,31,34,35}
        size=sum(1 if i in nullable else 4 for i in range(36))
        fields=bytearray(base[68:68+size]);fields[:4]=u32(aid)
        critter=['I','I','a','a','a','a','I','I','I','I','a','a','H','I','H','H','H','H','H','I','a','I','I','a','L','I','I','I','I','I','I','a','a']
        npc=['I','H','I','H','H','I','I','a','I','L','L','I','I','I','H','I','I','a','a','a','I','I','I','a','a']
        header=u32(119)+struct.pack('<H22x',65535)+struct.pack('<H6xI12x',1,number)+u32(16)+bytes(20)
        return header+fields+b''.join(bytes(4) if kind=='I' else b'\0' for kind in critter+npc)
    resources += [('proto/017001.pro',npc_prototype(17001,0x28000000),False),
                  ('proto/017002.pro',npc_prototype(17002,0xc0000000),False),
                  ('art/monster/monster.mes',b'{0}{rat}',False),
                  ('art/monster/rat/ratuwxaa.art',art_file(rotations=8,frames=1),False)]
    resources += [
        ('art/tile/grsbse0a.art', art_file(frames=1), False),
        ('art/tile/wd1bse0a.art', art_file(frames=1), False),
        ('art/container/container.mes', b'{1001}{chest.art}', False),
        ('art/container/chest.art', art_file(frames=1), False),
    ]
    # Extend the same MES entries instead of adding duplicate DAT keys.
    resources = [(name, b'{0}{int ext}{17}{wi1 wd1}' if name=='art/wall/structure.mes'
                  else b'{0}{drt}{1}{grs}{2}{swr/b}{3}{dwr/f}{300}{tst}{304}{wd1}{400}{drtgrs}{401}{drtswr}{402}{swrdwr}' if name=='art/tile/tilename.mes'
                  else data, compressed) for name,data,compressed in resources]
    for prefix in ('wi1','wd1'):
        for piece in ('bse','d3l','d3a','d3r'):
            resources.append((f'art/wall/{prefix}{piece}u0.art',art_file(frames=1),False))
        resources.append((f'art/wall/{prefix}w3lu1.art',art_file(frames=1),False))
    bmp = struct.pack('<2sIHHI',b'BM',102,0,0,54) + struct.pack('<IiiHHIIiiII',40,4,4,1,24,0,48,0,0,0,0) + bytes([60,110,80])*16
    terrain = struct.pack('<fIqqii',1.2,1,4,4,0,0)
    for y in range(4):
        row=zlib.compress(struct.pack('<4H',60,60,60,60)); terrain += u32(len(row))+row
    resources += [
        ('rules/maplist.mes',b'{5000}{test-world, 130, 90, Type: START_MAP, WorldMap: 0}',False),
        ('worldmap/worldmap.mes',b'{50}{1,1,testmap}',False),
        ('worldmap/testmap001.bmp',bmp,False),
        ('maps/test-world/map.prp',struct.pack('<iiqq',0,0,256,256),False),
        ('maps/test-world/67108866.sec',data,False),
        ('maps/test-world/terrain.tdf',terrain,False),
        ('terrain/terrain.mes',b'{0}{grass}',False),
        ('terrain/grass/0.sec',data,False),
    ]
    game = root / 'game'; game.mkdir()
    (game / 'arcanum2.dat').write_bytes(archive(resources))
    subprocess.run([sys.argv[1], str(game), str(root / 'window.png')],
                   env={**os.environ, 'QT_QPA_PLATFORM': 'offscreen'}, check=True, timeout=45)
