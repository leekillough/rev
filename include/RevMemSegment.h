//
// _RevCustomMem_h_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef REV_CUSTOM_MEM_H
#define REV_CUSTOM_MEM_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <tuple>
#include <utility>

#include "RevMemCtrl.h"
#include "SST.h"
#include "RevCommon.h"
#include "sst/core/output.h"

namespace SST::RevCPU {

// Forward declare RevCPU
class RevCPU;

/* Virtual Memory Blocks  */
class MemSegment {
public:
  MemSegment(uint64_t baseAddr, uint64_t size)
    : BaseAddr(baseAddr), Size(size) {
    TopAddr = baseAddr + size;
  }

  uint64_t getTopAddr() const { return BaseAddr + Size; }
  uint64_t getBaseAddr() const { return BaseAddr; }
  uint64_t getSize() const { return Size; }

  void setBaseAddr(uint64_t baseAddr) {
    BaseAddr = baseAddr;
    if( Size ){
      TopAddr = Size + BaseAddr;
    }
  }

  void setSize(uint64_t size) { Size = size; TopAddr = BaseAddr + size; }

  /// MemSegment: Check if vAddr is included in this segment
  bool contains(const uint64_t& vAddr){
    return (vAddr >= BaseAddr && vAddr < TopAddr);
  };

  // Check if a given range is inside a segment
  bool contains(const uint64_t& vBaseAddr, const uint64_t& Size){
    // exclusive top address
    uint64_t vTopAddr = vBaseAddr + Size - 1;
    return (this->contains(vBaseAddr) && this->contains(vTopAddr));
  };


  // Override for easy std::cout << *Seg << std::endl;
  friend std::ostream& operator<<(std::ostream& os, const MemSegment& Seg) {
    return os << " | BaseAddr:  0x" << std::hex << Seg.getBaseAddr() << " | TopAddr: 0x" << std::hex << Seg.getTopAddr() << " | Size: " << std::dec << Seg.getSize() << " Bytes";
  }

private:
  uint64_t BaseAddr;
  uint64_t Size;
  uint64_t TopAddr;
};

// TODO: Potentially make the constructor a std::variant and then allow for arbitrary arguments
// to be passed via the python config so long as they are a part of the 'scoped' parameter name
// of the segment... I think we'd have to make some assumptions about the ordering being constant
//
// TODO: Also, make the python config potentially treat the segment name as a type instead of a name and
// allow the user to name the segment (ie. scratchpad1)?
//
// TODO: Allow for only certain Proc/Harts to access?
// TODO: Improve the printing override

class RevCustomMemSegment : public MemSegment {
public:
   RevCustomMemSegment(RevCPU* CPU, uint64_t baseAddr, size_t size, const std::string& name,
                     SST::Output *output)
        : MemSegment(baseAddr, size), Name(name), CPU(CPU), output(output) {
      // TODO: Figure out if this is how I want to do this
    }

    // All custom memory segments must override the read & write functions
    virtual void ReadMem(unsigned Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req, RevFlag flags) = 0;
    virtual void WriteMem(unsigned Hart, uint64_t Addr, size_t Len, const void *Data, RevFlag flags) = 0;

    // Optional virtual method for implementing some sort of virtual addressing scheme
    // exclusive to this address space
    virtual uint64_t CalcPhysAddr(const uint64_t Addr);

    // Optional virtual methods for overriding allocation / deallocation behavior within a custom segment
    // this allows for things like mmap to work by default with these custom segments
    virtual uint64_t Alloc(const unsigned Hart, const size_t Bytes){
      output->fatal(CALL_INFO, 11, "Attempted to perform an allocation inside a custom memory segment <%s> "
                                   "however this type of segment does not have an implementation for "
                                   "Alloc defined.\n", Name.c_str());
      return 0;
    }

    virtual void Dealloc(const unsigned Hart, const uint64_t BaseAddr, const size_t Bytes){
      output->fatal(CALL_INFO, 11, "Attempted to perform a deallocation inside a custom memory segment <%s> "
                                   "however this type of segment does not have an implementation for "
                                   "Dealloc defined.\n", Name.c_str());
      return;
    }

    // Optional initialization function
    template <typename Func, typename... Args>
    void SetInitFunction(Func&& func, Args&&... args) {
        InitFunc = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    virtual void Initialize() {
      output->verbose(CALL_INFO, 2, 1, "Initializing a custom memory segment <%s>\n", Name.c_str());
      if (InitFunc) {
          InitFunc();
      }
    }

protected:
    std::string Name;
    RevCPU* CPU;
    SST::Output* output;

private:
    std::function<void()> InitFunc;
};
} // namespace SST::RevCPU

#endif
