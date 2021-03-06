// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPH_REFCOUNTEDOBJ_H
#define CEPH_REFCOUNTEDOBJ_H

#include "common/ceph_mutex.h"
#include "common/ceph_context.h"
#include "common/valgrind.h"
#include "common/debug.h"

#include <boost/smart_ptr/intrusive_ptr.hpp>

// re-include our assert to clobber the system one; fix dout:
#include "include/ceph_assert.h"

///< 2019/3/3, 索引对象, 提供计数索引功能, 类似智能指针方式, 消息会继承该类

///< mutable属于c++关键字: 1、表示可变的意思; 2、为了突破const关键字的限制, 被mutable修饰的关键字用于处于变化的状态;

struct RefCountedObject {
private:
  mutable std::atomic<uint64_t> nref;   // mutable修饰nref, 即使在const函数中, nref也是可以被修改的;
  CephContext *cct;
public:
  RefCountedObject(CephContext *c = NULL, int n=1) : nref(n), cct(c) {}
  virtual ~RefCountedObject() {
    ceph_assert(nref == 0);
  }

  // 使用者加1
  const RefCountedObject *get() const {
    int v = ++nref;
    if (cct)
      lsubdout(cct, refs, 1) << "RefCountedObject::get " << this << " "
			     << (v - 1) << " -> " << v
			     << dendl;
    return this;
  }
  RefCountedObject *get() {
    int v = ++nref;
    if (cct)
      lsubdout(cct, refs, 1) << "RefCountedObject::get " << this << " "
			     << (v - 1) << " -> " << v
			     << dendl;
    return this;
  }

  // 使用者减1
  void put() const {
    CephContext *local_cct = cct;
    int v = --nref;
    if (local_cct)
      lsubdout(local_cct, refs, 1) << "RefCountedObject::put " << this << " "
				   << (v + 1) << " -> " << v
				   << dendl;
    if (v == 0) {
      ANNOTATE_HAPPENS_AFTER(&nref);
      ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(&nref);
      delete this;
    } else {
      ANNOTATE_HAPPENS_BEFORE(&nref);
    }
  }

  void set_cct(CephContext *c) {
    cct = c;
  }

  uint64_t get_nref() const {
    return nref;
  }
};

#ifndef WITH_SEASTAR

/**
 * RefCountedCond
 *
 *  a refcounted condition, will be removed when all references are dropped
 */

///< std::unique_lock和std::lock_guard离开作用域后, 均会自动释放锁
///< std::unique_lock还支持lock和unlock操作来手动释放锁

struct RefCountedCond : public RefCountedObject {
  bool complete;
  ceph::mutex lock = ceph::make_mutex("RefCountedCond::lock");
  ceph::condition_variable cond;
  int rval;

  RefCountedCond() : complete(false), rval(0) {}

  int wait() {
    std::unique_lock l(lock);
    while (!complete) {
      cond.wait(l);
    }
    return rval;
  }

  void done(int r) {
    std::lock_guard l(lock);
    rval = r;
    complete = true;
    cond.notify_all();
  }

  void done() {
    done(0);
  }
};

/**
 * RefCountedWaitObject
 *
 * refcounted object that allows waiting for the object's last reference.
 * Any referrer can either put or put_wait(). A simple put() will return
 * immediately, a put_wait() will return only when the object is destroyed.
 * e.g., useful when we want to wait for a specific event completion. We
 * use RefCountedCond, as the condition can be referenced after the object
 * destruction.
 *
 */
struct RefCountedWaitObject {
  std::atomic<uint64_t> nref = { 1 };
  RefCountedCond *c;

  RefCountedWaitObject() {
    c = new RefCountedCond;
  }
  virtual ~RefCountedWaitObject() {
    c->put();
  }

  RefCountedWaitObject *get() {
    nref++;
    return this;
  }

  bool put() {
    bool ret = false;
    RefCountedCond *cond = c;
    cond->get();
    if (--nref == 0) {
      cond->done();
      delete this;
      ret = true;
    }
    cond->put();
    return ret;
  }

  void put_wait() {
    RefCountedCond *cond = c;

    cond->get();
    if (--nref == 0) {
      cond->done();
      delete this;
    } else {
      cond->wait();
    }
    cond->put();
  }
};

#endif // WITH_SEASTAR

static inline void intrusive_ptr_add_ref(const RefCountedObject *p) {
  p->get();
}
static inline void intrusive_ptr_release(const RefCountedObject *p) {
  p->put();
}

using RefCountedPtr = boost::intrusive_ptr<RefCountedObject>;

///< boost::intrusive_ptr一种“侵入式”的引用计数指针，它实际并不提供引用计数功能，而是要求被存储的对象自己实现引用计数功能
///< 并提供intrusive_ptr_add_ref和intrusive_ptr_release函数接口供boost::intrusive_ptr调用

#endif
