// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include "Common/FileUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/MemoryWatcher.h"

MemoryWatcher::MemoryWatcher()
{
  m_running = false;
  if (!LoadAddresses(File::GetUserPath(F_MEMORYWATCHERLOCATIONS_IDX)))
  {
    std::cout << "Failed to load MemoryWatcher addresses. Not watching memory." << std::endl;
    return;
  }
  if (!OpenSocket(File::GetUserPath(F_MEMORYWATCHERSOCKET_IDX)))
    return;
  m_running = true;
}

MemoryWatcher::~MemoryWatcher()
{
  if (!m_running)
    return;

  m_running = false;

#ifndef USE_ZMQ
  close(m_fd);
#else
  zmq_close(m_socket);
  zmq_ctx_destroy(m_context);
#endif
}

bool MemoryWatcher::LoadAddresses(const std::string& path)
{
  std::ifstream locations;
  File::OpenFStream(locations, path, std::ios_base::in);
  if (!locations)
  {
    std::cout << "No MemoryWatcher Locations." << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(locations, line))
    ParseLine(line);

  return !m_values.empty();
}

void MemoryWatcher::ParseLine(const std::string& line)
{
  m_values[line] = 0;
  m_addresses[line] = std::vector<u32>();

  std::stringstream offsets(line);
  offsets >> std::hex;
  u32 offset;
  while (offsets >> offset)
    m_addresses[line].push_back(offset);
}

#ifdef USE_ZMQ
static const char* ZMQErrorString()
{
  return zmq_strerror(zmq_errno());
}
#endif

bool MemoryWatcher::OpenSocket(const std::string& path)
{
#ifndef USE_ZMQ
  m_addr.sun_family = AF_UNIX;
  strncpy(m_addr.sun_path, path.c_str(), sizeof(m_addr.sun_path) - 1);

  m_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  return m_fd >= 0;
#else
  std::cout << "Connecting zmq socket to " << path << std::endl;
  
  if (!File::Exists(path))
  {
    std::cout << "Socket does not exist!" << std::endl;
    return false;
  }
  
  if (!(m_context = zmq_ctx_new()))
  {
    std::cout << "Failed to allocate zmq context: " << ZMQErrorString() << std::endl;
    return false;
  }

  if(!(m_socket = zmq_socket(m_context, ZMQ_REQ)))
  {
    std::cout << "Failed to create zmq socket: " << ZMQErrorString() << std::endl;
    return false;
  }
  
  if(zmq_connect(m_socket, ("ipc://" + path).c_str()) < 0)
  {
    std::cout << "Error connecting socket: " << ZMQErrorString() << std::endl;
    return false;
  }
  
  std::cout << "Connected zmq socket to " << path << std::endl;
  return true;
#endif
}

u32 MemoryWatcher::ChasePointer(const std::string& line)
{
  u32 value = 0;
  for (u32 offset : m_addresses[line])
    value = Memory::Read_U32(value + offset);
  return value;
}

std::string MemoryWatcher::ComposeMessages()
{
  std::stringstream message_stream;
  // prevent silly things like commas being added!
  message_stream.imbue(std::locale::classic());
  message_stream << std::hex;

  for (auto& entry : m_values)
  {
    std::string address = entry.first;
    u32& current_value = entry.second;

    u32 new_value = ChasePointer(address);
    if (new_value != current_value)
    {
      // Update the value
      current_value = new_value;
      message_stream << address << '\n' << new_value << '\n';
    }
  }

  return message_stream.str();
}

void MemoryWatcher::Step()
{
  if (!m_running)
    return;

  std::string message = ComposeMessages();
#ifndef USE_ZMQ
  sendto(m_fd, message.c_str(), message.size() + 1, 0,
         reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr));
#else
  zmq_send(m_socket, message.c_str(), message.size(), 0);

  // do something with the reply?
  char buffer[10];
  zmq_recv(m_socket, buffer, 10, 0);
#endif
}

