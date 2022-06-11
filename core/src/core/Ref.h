#pragma once

#include <assert.h>
#include <memory>
#include <stdint.h>


class RefCounted {
public:
   void IncRefCount() const {
      m_RefCount++;
   }

   void DecRefCount() const {
      assert(m_RefCount > 0);
      m_RefCount--;
   }

   uint32_t GetRefCount() const { return m_RefCount; }

   RefCounted& operator=(const RefCounted&) = delete;

private:
   mutable uint32_t m_RefCount = 0; // TODO: atomic
};

template <typename T>
class Ref {
public:
   Ref()
      : m_Instance(nullptr) {
   }

   Ref(std::nullptr_t n)
      : m_Instance(nullptr) {
   }

   Ref(T* instance)
      : m_Instance(instance) {
      static_assert(std::is_base_of<RefCounted, T>::value, "Class is not RefCounted!");

      IncRef();
   }

   template <typename T2>
   Ref(const Ref<T2>& other) {
      m_Instance = (T*)other.m_Instance;
      IncRef();
   }

   template <typename T2>
   Ref(Ref<T2>&& other) {
      m_Instance = (T*)other.m_Instance;
      other.m_Instance = nullptr;
   }

   ~Ref() {
      DecRef();
   }

   Ref(const Ref<T>& other)
      : m_Instance(other.m_Instance) {
      IncRef();
   }

   Ref& operator=(std::nullptr_t) {
      DecRef();
      m_Instance = nullptr;
      return *this;
   }

   Ref& operator=(const Ref<T>& other) {
      other.IncRef();
      DecRef();

      m_Instance = other.m_Instance;
      return *this;
   }

   template <typename T2>
   Ref& operator=(const Ref<T2>& other) {
      other.IncRef();
      DecRef();

      m_Instance = other.m_Instance;
      return *this;
   }

   template <typename T2>
   Ref& operator=(Ref<T2>&& other) {
      DecRef();

      m_Instance = other.m_Instance;
      other.m_Instance = nullptr;
      return *this;
   }

   operator bool() { return m_Instance != nullptr; }
   operator bool() const { return m_Instance != nullptr; }

   T* operator->() { return m_Instance; }
   const T* operator->() const { return m_Instance; }

   T& operator*() { return *m_Instance; }
   const T& operator*() const { return *m_Instance; }

   T* Raw() { return m_Instance; }
   const T* Raw() const { return m_Instance; }

   void Reset(T* instance = nullptr) {
      DecRef();
      m_Instance = instance;
   }

   template <typename... Args>
   static Ref<T> Create(Args&&... args) {
      return Ref<T>(new T(std::forward<Args>(args)...));
   }

private:
   void IncRef() const {
      if (m_Instance)
         m_Instance->IncRefCount();
   }

   void DecRef() const {
      if (m_Instance) {
         m_Instance->DecRefCount();
         if (m_Instance->GetRefCount() == 0) {
            delete m_Instance;
         }
      }
   }

   template <class T2>
   friend class Ref;
   T* m_Instance;
};

template<typename T>
using Scope = std::unique_ptr<T>;
