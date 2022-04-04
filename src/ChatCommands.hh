#pragma once

#include <stdint.h>

#include <memory>
#include <string>

#include "ServerState.hh"
#include "Lobby.hh"
#include "Client.hh"

const std::string& name_for_section_id(uint8_t section_id);
std::u16string u16name_for_section_id(uint8_t section_id);
uint8_t section_id_for_name(const std::string& name);
uint8_t section_id_for_name(const std::u16string& name);

const std::string& name_for_event(uint8_t event);
std::u16string u16name_for_event(uint8_t event);
uint8_t event_for_name(const std::string& name);
uint8_t event_for_name(const std::u16string& name);

const std::string& name_for_lobby_type(uint8_t type);
std::u16string u16name_for_lobby_type(uint8_t type);
uint8_t lobby_type_for_name(const std::string& name);
uint8_t lobby_type_for_name(const std::u16string& name);

const std::string& name_for_technique(uint8_t tech);
std::u16string u16name_for_technique(uint8_t tech);
uint8_t technique_for_name(const std::string& name);
uint8_t technique_for_name(const std::u16string& name);

const std::string& name_for_npc(uint8_t npc);
std::u16string u16name_for_npc(uint8_t npc);
uint8_t npc_for_name(const std::string& name);
uint8_t npc_for_name(const std::u16string& name);

void process_chat_command(std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, const std::u16string& text);
