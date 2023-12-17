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
#include "sst/core/params.h"

// TODO: Idk how we would do it but potentially add
//       a switch that allows this to interact with the TLB

namespace SST::RevCPU {

// Forward declare RevCPU
class RevCPU;

// Virtual Memory Blocks
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

  void setSize(uint64_t size) { Size = size; TopAddr = BaseAddr + size - 1; }

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

  ///< RevMemSegment: Overload the ostream operator
  friend std::ostream& operator<<(std::ostream& os, const MemSegment& Seg) {
    return os << " | BaseAddr:  0x" << std::hex << Seg.getBaseAddr() << " | TopAddr: 0x" << std::hex << Seg.getTopAddr() << " | Size: " << std::dec << Seg.getSize() << " Bytes";
  }

  ///< RevMemSegment: Overload the to_string method
  std::string to_string() const {
    std::ostringstream oss;
    oss << *this;      // << operator is overloaded above
    return oss.str();  // Return the string
  }

private:
  uint64_t BaseAddr;
  uint64_t Size;
  uint64_t TopAddr;
};

class RevCustomMemSegment : public MemSegment {
public:
   RevCustomMemSegment(const std::string& Type, const std::string& Name,
                       RevCPU* CPU, SST::Params& Params, SST::Output *output)
        : MemSegment(0,0), Type(Type), Name(Name), CPU(CPU), output(output), Params(Params) {

    // Setup whatever needs to be setup (default, un-overriden behavior includes setting size, baseAddr, and name)
    // ParseAdditionalParams();
  }

  // Virtual function that can be overridden allowing for custom attributes to be passed
  // from the python file and handled during the creation of the this custom memory segment
  virtual void ParseAdditionalParams(){
    // The default behavior is to look for the size and baseAddr of the segment
    // TODO: Figure out if we want to require everything to have a baseAddr & size or not
    //       If so, we can probably just move this to the constructor and make it non-virtual
    //       ... We could also make a ParseMandatoryParams() function that is non-virtual
    size_t segSize = Params.find<uint64_t>("size", 0);
    uint64_t segBaseAddr = Params.find<size_t>("baseAddr", 0);

    // Make sure we found them
    if( !segSize ){
      // We couldn't find the size/baseAddr for segments, throw error
      output->fatal(CALL_INFO, -1,
                    "Custom memory segment '%s' was found with an invalid size "
                    "or none at all: [%zu Bytes]. \n\nPlease specify the segment size "
                    "as a scoped parameter prefixed by the name of the custom segment "
                    "you previously specified (ex. \"%s.size\")\n\n",
                    Name.c_str(), segSize, Name.c_str());
    }
    // Make sure we found them
    if( !segBaseAddr ){
      // We couldn't find the size/baseAddr for segments, throw error
      output->fatal(CALL_INFO, -1,
                    "Custom memory segment '%s' was found starting at an invalid "
                    "base address or none at all: [0x%" PRIx64 "]. \n\nPlease specify "
                    "the base address of the segment as a scoped parameter "
                    "prefixed by the name of the corresponding segment (ex. \"%s.baseAddr\")\n\n",
                    Name.c_str(), segBaseAddr, Name.c_str());
      }

    // Make sure we found them
    if( !segBaseAddr ){
      // We couldn't find the size/baseAddr for segments, throw error
      output->fatal(CALL_INFO, -1,
                    "Custom memory segment '%s' was found starting at an invalid "
                    "base address or none at all: [0x%" PRIx64 "]. \n\nPlease specify "
                    "the base address of the segment as a scoped parameter "
                    "prefixed by the name of the corresponding segment (ex. \"%s.baseAddr\")\n\n",
                    Name.c_str(), segBaseAddr, Name.c_str());
    }
    setBaseAddr(segBaseAddr);
    setSize(segSize);
  };

  // All custom memory segments must override the read & write functions
  virtual void ReadMem(unsigned Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req, RevFlag flags) = 0;
  virtual void WriteMem(unsigned Hart, uint64_t Addr, size_t Len, const void *Data, RevFlag flags) = 0;

  // Optional virtual method for implementing some sort of virtual addressing scheme
  // exclusive to this address space
  virtual uint64_t CalcPhysAddr(const uint64_t Addr){
    return Addr;
  }

  // Optional virtual methods for overriding allocation / deallocation behavior within a custom segment
  // this allows for things like mmap to work by default with these custom segments
  virtual uint64_t Alloc(const unsigned Hart, const size_t Bytes){
    output->fatal(CALL_INFO, 11, "Attempted to perform an allocation inside a custom memory segment <%s> "
                                  "however this type of segment does not have an implementation for "
                                  "Alloc defined.\n", Name.c_str());
    return 0;
  }

  // Override for allocating at a specific address
  virtual uint64_t AllocAt(const unsigned Hart, const size_t Bytes, const uint64_t BaseAddr){
    output->fatal(CALL_INFO, 11, "Attempted to perform an allocation inside a custom memory segment <%s> "
                                  "however this type of segment does not have an implementation for "
                                  "AllocAt defined.\n", Name.c_str());
    return 0;
  }

  virtual void Dealloc(const unsigned Hart, const uint64_t Addr, const size_t Size){
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

  // Override for ostream operator
  // TODO: Make pretty
  friend std::ostream& operator<<(std::ostream& os, const RevCustomMemSegment& seg) {
    os << static_cast<const MemSegment&>(seg);
    os << " | Name: " << seg.Name << "\n";
    seg.Params.print_all_params(os);
    return os;
  }

  ///< RevCustomMemSegment: Overload the to_string method
  std::string to_string() const {
    std::ostringstream oss;
    oss << *this;      // << operator is overloaded above
    return oss.str();  // Return the string
  }

protected:
  std::string Type;
  std::string Name;
  RevCPU* CPU;
  SST::Output* output;
  SST::Params& Params;

private:
  // TODO: Probably delete
  std::function<void()> InitFunc;
};
} // namespace SST::RevCPU

#endif
