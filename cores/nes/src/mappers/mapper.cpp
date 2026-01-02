#include "mapper.hpp"
#include "mapper_000.hpp"
#include "mapper_001.hpp"
#include "mapper_002.hpp"
#include "mapper_003.hpp"
#include "mapper_004.hpp"
#include "mapper_005.hpp"
#include "mapper_007.hpp"
#include "mapper_009.hpp"
#include "mapper_010.hpp"
#include "mapper_011.hpp"
#include "mapper_016.hpp"
#include "mapper_019.hpp"
#include "mapper_020.hpp"
#include "mapper_024.hpp"
#include "mapper_034.hpp"
#include "mapper_066.hpp"
#include "mapper_069.hpp"
#include "mapper_071.hpp"
#include "mapper_079.hpp"
#include "mapper_085.hpp"
#include "mapper_206.hpp"
#include "mapper_vrc.hpp"
#include <iostream>

namespace nes {

Mapper* create_mapper(int mapper_number,
                      std::vector<uint8_t>& prg_rom,
                      std::vector<uint8_t>& chr_rom,
                      std::vector<uint8_t>& prg_ram,
                      MirrorMode initial_mirror,
                      bool has_chr_ram)
{
    switch (mapper_number) {
        case 0:
            return new Mapper000(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 1:
            return new Mapper001(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 2:
            return new Mapper002(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 3:
            return new Mapper003(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 4:
            return new Mapper004(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 5:
            return new Mapper005(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 7:
            return new Mapper007(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 9:
            return new Mapper009(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 10:
            return new Mapper010(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 11:
            return new Mapper011(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 16:
            // Mapper 16: Bandai FCG with EEPROM (24C02 default for Dragon Ball Z games)
            return new Mapper016(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram,
                                 Mapper016::EepromType::EEPROM_24C02);
        case 19:
            return new Mapper019(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 20:
            // FDS - Famicom Disk System
            return new Mapper020(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 21:
            return new Mapper021(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 22:
            return new Mapper022(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 23:
            return new Mapper023(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 24:
            return new Mapper024(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 25:
            return new Mapper025(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 26:
            return new Mapper026(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 34:
            return new Mapper034(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 66:
            return new Mapper066(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 69:
            return new Mapper069(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 71:
            return new Mapper071(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 79:
            return new Mapper079(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 85:
            return new Mapper085(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        case 206:
            return new Mapper206(prg_rom, chr_rom, prg_ram, initial_mirror, has_chr_ram);
        default:
            std::cerr << "Unsupported mapper: " << mapper_number << std::endl;
            return nullptr;
    }
}

} // namespace nes
