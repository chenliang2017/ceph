// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LOG_SUBSYSTEMS
#define CEPH_LOG_SUBSYSTEMS

#include <string>
#include <vector>

#include "include/assert.h"

namespace ceph {
namespace logging {

// SubsystemΪ��ϵͳ����־���������

struct Subsystem {
  int log_level, gather_level; // log_levelΪ��־��ӡ����, ���ڸü������־����ˢ����־��
  std::string name;         // gather_levelΪ��־ͳ�Ƽ���, ���ڸü������־���ռ�����,
  // nameΪ��ģ�������ceph_subsys_xxx     // ��Log����־����

  Subsystem() : log_level(0), gather_level(0) {}
};

class SubsystemMap {
  std::vector<Subsystem> m_subsys;  // ��¼������ģ�����־��������
  unsigned m_max_name_len;

  friend class Log;

public:
  SubsystemMap() : m_max_name_len(0) {}

  size_t get_num() const {
    return m_subsys.size();
  }

  int get_max_subsys_len() const {
    return m_max_name_len;
  }

  void add(unsigned subsys, std::string name, int log, int gather);
  void set_log_level(unsigned subsys, int log);
  void set_gather_level(unsigned subsys, int gather);

  int get_log_level(unsigned subsys) const {
    if (subsys >= m_subsys.size())
      subsys = 0;
    return m_subsys[subsys].log_level;
  }

  int get_gather_level(unsigned subsys) const {
    if (subsys >= m_subsys.size())
      subsys = 0;
    return m_subsys[subsys].gather_level;
  }

  const std::string& get_name(unsigned subsys) const {
    if (subsys >= m_subsys.size())
      subsys = 0;
    return m_subsys[subsys].name;
  }

  // �Ƿ���Ҫ�ռ����������־
  bool should_gather(unsigned sub, int level) {
    assert(sub < m_subsys.size());
    return level <= m_subsys[sub].gather_level ||
      level <= m_subsys[sub].log_level;
  }
};

}
}

#endif
