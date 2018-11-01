#include "PSOProtocol.hh"



uint16_t PSOCommandHeader::command(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
    case GameVersion::GC:
      return reinterpret_cast<const PSOCommandHeaderDCGC*>(this)->command;
    case GameVersion::PC:
    case GameVersion::Patch:
      return reinterpret_cast<const PSOCommandHeaderPC*>(this)->command;
    case GameVersion::BB:
      return reinterpret_cast<const PSOCommandHeaderBB*>(this)->command;
  }
}

uint16_t PSOCommandHeader::size(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
    case GameVersion::GC:
      return reinterpret_cast<const PSOCommandHeaderDCGC*>(this)->size;
    case GameVersion::PC:
    case GameVersion::Patch:
      return reinterpret_cast<const PSOCommandHeaderPC*>(this)->size;
    case GameVersion::BB:
      return reinterpret_cast<const PSOCommandHeaderBB*>(this)->size;
  }
}

uint32_t PSOCommandHeader::flag(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
    case GameVersion::GC:
      return reinterpret_cast<const PSOCommandHeaderDCGC*>(this)->flag;
    case GameVersion::PC:
    case GameVersion::Patch:
      return reinterpret_cast<const PSOCommandHeaderPC*>(this)->flag;
    case GameVersion::BB:
      return reinterpret_cast<const PSOCommandHeaderBB*>(this)->flag;
  }
}

