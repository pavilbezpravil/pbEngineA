#pragma once

#include <assert.h>
#include <memory>
#include <stdint.h>

#include "Core.h"

namespace pbe {

   class CORE_API RefCounted {
   public:
      void IncRefCount() const {
         refCount++;
      }

      void DecRefCount() const {
         assert(refCount > 0);
         refCount--;
      }

      uint32_t GetRefCount() const { return refCount; }

      RefCounted& operator=(const RefCounted&) = delete;

   private:
      mutable uint32_t refCount = 0;
   };

   template <typename T>
   class Ref {
   public:
      Ref() : instance(nullptr) {}

      Ref(std::nullptr_t n)
         : instance(nullptr) {
      }

      Ref(T* instance) : instance(instance) {
         static_assert(std::is_base_of<RefCounted, T>::value, "Class is not RefCounted!");

         IncRef();
      }

      template <typename T2>
      Ref(const Ref<T2>& other) {
         instance = (T*)other.instance;
         IncRef();
      }

      template <typename T2>
      Ref(Ref<T2>&& other) {
         instance = (T*)other.instance;
         other.instance = nullptr;
      }

      ~Ref() {
         DecRef();
      }

      Ref(const Ref<T>& other)
         : instance(other.instance) {
         IncRef();
      }

      Ref& operator=(std::nullptr_t) {
         DecRef();
         instance = nullptr;
         return *this;
      }

      Ref& operator=(const Ref<T>& other) {
         other.IncRef();
         DecRef();

         instance = other.instance;
         return *this;
      }

      template <typename T2>
      Ref& operator=(const Ref<T2>& other) {
         other.IncRef();
         DecRef();

         instance = other.instance;
         return *this;
      }

      template <typename T2>
      Ref& operator=(Ref<T2>&& other) {
         DecRef();

         instance = other.instance;
         other.instance = nullptr;
         return *this;
      }

      operator bool() { return instance != nullptr; }
      operator bool() const { return instance != nullptr; }

      T* operator->() { return instance; }
      const T* operator->() const { return instance; }

      T& operator*() { return *instance; }
      const T& operator*() const { return *instance; }

      T* Raw() { return instance; }
      const T* Raw() const { return instance; }

      operator T* () { return instance; }

      void Reset(T* instance_ = nullptr) {
         DecRef();
         instance = instance_;
      }

      template <typename... Args>
      static Ref<T> Create(Args&&... args) {
         return Ref<T>(new T(std::forward<Args>(args)...));
      }

   private:
      void IncRef() const {
         if (instance)
            instance->IncRefCount();
      }

      void DecRef() const {
         if (instance) {
            instance->DecRefCount();
            if (instance->GetRefCount() == 0) {
               delete instance;
            }
         }
      }

      template <class T2>
      friend class Ref;
      T* instance;
   };

   template<typename T>
   using Own = std::unique_ptr<T>;

}
