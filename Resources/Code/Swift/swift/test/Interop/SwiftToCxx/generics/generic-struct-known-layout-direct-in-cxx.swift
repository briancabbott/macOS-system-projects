// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend %S/generic-struct-in-cxx.swift -D KNOWN_LAYOUT -typecheck -module-name Generics -clang-header-expose-decls=all-public -emit-clang-header-path %t/generics.h
// RUN: %FileCheck %s < %t/generics.h
// RUN: %check-generic-interop-cxx-header-in-clang(%t/generics.h -Wno-reserved-identifier)

// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend %S/generic-struct-in-cxx.swift -D KNOWN_LAYOUT -enable-library-evolution -typecheck -module-name Generics -clang-header-expose-decls=all-public -emit-clang-header-path %t/generics.h
// RUN: %FileCheck %s < %t/generics.h
// RUN: %check-generic-interop-cxx-header-in-clang(%t/generics.h -Wno-reserved-identifier)

// CHECK: // Stub struct to be used to pass/return values to/from Swift functions.
// CHECK-NEXT: struct swift_interop_passStub_Generics_[[PTRPTRENC:void_ptr_[0-9]_[0-9]_void_ptr_[0-9]_[0-9]+]] {
// CHECK-NEXT:  void * _Nullable _1;
// CHECK-NEXT:  void * _Nullable _2;
// CHECK-NEXT: };
// CHECK-EMPTY:
// CHECK-NEXT: static inline struct swift_interop_passStub_Generics_[[PTRPTRENC]] swift_interop_passDirect_Generics_[[PTRPTRENC]](const char * _Nonnull value) __attribute__((always_inline)) {
// CHECK-NEXT:  struct swift_interop_passStub_Generics_[[PTRPTRENC]] result;
// CHECK-NEXT:  memcpy(&result._1, value
// CHECK-NEXT:  memcpy(&result._2, value
// CHECK-NEXT:  return result;
// CHECK-NEXT:}
// CHECK-EMPTY:
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV1yq_vg(SWIFT_INDIRECT_RESULT void * _Nonnull, struct swift_interop_passStub_Generics_[[PTRPTRENC]] _self, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV1yq_vs(const void * _Nonnull newValue, void * _Nonnull , SWIFT_CONTEXT void * _Nonnull _self) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: // Stub struct to be used to pass/return values to/from Swift functions.
// CHECK-NEXT: struct swift_interop_returnStub_Generics_[[PTRPTRENC]] {
// CHECK-NEXT:   void * _Nullable _1;
// CHECK-NEXT:   void * _Nullable _2;
// CHECK-NEXT: };
// CHECK-EMPTY:
// CHECK-NEXT: static inline void swift_interop_returnDirect_Generics_[[PTRPTRENC]](char * _Nonnull result, struct swift_interop_returnStub_Generics_[[PTRPTRENC]] value) __attribute__((always_inline)) {
// CHECK-NEXT:   memcpy(result
// CHECK-NEXT:   memcpy(result
// CHECK-NEXT: }
// CHECK-EMPTY:
// CHECK-NEXT: SWIFT_EXTERN struct swift_interop_returnStub_Generics_[[PTRPTRENC]] $s8Generics11GenericPairVyACyxq_Gx_Siq_tcfC(const void * _Nonnull x, ptrdiff_t i, const void * _Nonnull y, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // init(_:_:_:)
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV6methodyyF(struct swift_interop_passStub_Generics_[[PTRPTRENC]] _self, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // method()
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV14mutatingMethodyyACyq_xGF(struct swift_interop_passStub_Generics_[[PTRPTRENC]] other, void * _Nonnull , SWIFT_CONTEXT void * _Nonnull _self) SWIFT_NOEXCEPT SWIFT_CALL; // mutatingMethod(_:)
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV13genericMethodyqd__qd___q_tlF(SWIFT_INDIRECT_RESULT void * _Nonnull, const void * _Nonnull x, const void * _Nonnull y, struct swift_interop_passStub_Generics_[[PTRPTRENC]] _self, void * _Nonnull , void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // genericMethod(_:_:)
// CHECK-NEXT: SWIFT_EXTERN ptrdiff_t $s8Generics11GenericPairV12computedPropSivg(struct swift_interop_passStub_Generics_[[PTRPTRENC]] _self, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV11computedVarxvg(SWIFT_INDIRECT_RESULT void * _Nonnull, struct swift_interop_passStub_Generics_[[PTRPTRENC]] _self, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics11GenericPairV11computedVarxvs(const void * _Nonnull newValue, void * _Nonnull , SWIFT_CONTEXT void * _Nonnull _self) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: // Stub struct to be used to pass/return values to/from Swift functions.
// CHECK-NEXT: struct swift_interop_passStub_Generics_uint64_t_0_8_uint64_t_8_16 {
// CHECK-NEXT:   uint64_t _1;
// CHECK-NEXT:   uint64_t _2;
// CHECK-NEXT: };
// CHECK-EMPTY:
// CHECK-NEXT: static inline struct swift_interop_passStub_Generics_uint64_t_0_8_uint64_t_8_16 swift_interop_passDirect_Generics_uint64_t_0_8_uint64_t_8_16(const char * _Nonnull value) __attribute__((always_inline)) {
// CHECK: }
// CHECK-EMPTY:
// CHECK-NEXT: SWIFT_EXTERN uint64_t $s8Generics12PairOfUInt64V1xs0D0Vvg(struct swift_interop_passStub_Generics_uint64_t_0_8_uint64_t_8_16 _self) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: SWIFT_EXTERN uint64_t $s8Generics12PairOfUInt64V1ys0D0Vvg(struct swift_interop_passStub_Generics_uint64_t_0_8_uint64_t_8_16 _self) SWIFT_NOEXCEPT SWIFT_CALL; // _
// CHECK-NEXT: // Stub struct to be used to pass/return values to/from Swift functions.
// CHECK-NEXT: struct swift_interop_returnStub_Generics_uint64_t_0_8_uint64_t_8_16 {
// CHECK-NEXT:   uint64_t _1;
// CHECK-NEXT:   uint64_t _2;
// CHECK-NEXT: };
// CHECK-EMPTY:
// CHECK-NEXT: static inline void swift_interop_returnDirect_Generics_uint64_t_0_8_uint64_t_8_16(char * _Nonnull result, struct swift_interop_returnStub_Generics_uint64_t_0_8_uint64_t_8_16 value) __attribute__((always_inline)) {
// CHECK: }

// CHECK: SWIFT_EXTERN void $s8Generics17inoutConcretePairyys6UInt16V_AA07GenericD0VyA2DGztF(uint16_t x, void * _Nonnull y) SWIFT_NOEXCEPT SWIFT_CALL; // inoutConcretePair(_:_:)
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics16inoutGenericPairyyAA0cD0Vyxq_Gz_xtr0_lF(void * _Nonnull x, const void * _Nonnull y, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // inoutGenericPair(_:_:)
// CHECK-NEXT: SWIFT_EXTERN struct swift_interop_returnStub_Generics_[[PTRPTRENC]] $s8Generics16makeConcretePairyAA07GenericD0Vys6UInt16VAFGAF_AFtF(uint16_t x, uint16_t y) SWIFT_NOEXCEPT SWIFT_CALL; // makeConcretePair(_:_:)
// CHECK-NEXT: SWIFT_EXTERN struct swift_interop_returnStub_Generics_[[PTRPTRENC]] $s8Generics15makeGenericPairyAA0cD0Vyxq_Gx_q_tr0_lF(const void * _Nonnull x, const void * _Nonnull y, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // makeGenericPair(_:_:)
// CHECK-NEXT: SWIFT_EXTERN struct swift_interop_returnStub_Generics_[[PTRPTRENC]] $s8Generics23passThroughConcretePair_1yAA07GenericE0Vys6UInt16VAGGAH_AGtF(struct swift_interop_passStub_Generics_[[PTRPTRENC]] x, uint16_t y) SWIFT_NOEXCEPT SWIFT_CALL; // passThroughConcretePair(_:y:)
// CHECK-NEXT: SWIFT_EXTERN struct swift_interop_returnStub_Generics_[[PTRPTRENC]] $s8Generics22passThroughGenericPairyAA0dE0Vyxq_GAE_q_tr0_lF(struct swift_interop_passStub_Generics_[[PTRPTRENC]] x, const void * _Nonnull y, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // passThroughGenericPair(_:_:)
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics16takeConcretePairyyAA07GenericD0Vys6UInt16VAFGF(struct swift_interop_passStub_Generics_[[PTRPTRENC]] x) SWIFT_NOEXCEPT SWIFT_CALL; // takeConcretePair(_:)
// CHECK-NEXT: SWIFT_EXTERN void $s8Generics15takeGenericPairyyAA0cD0Vyxq_Gr0_lF(struct swift_interop_passStub_Generics_[[PTRPTRENC]] x, void * _Nonnull , void * _Nonnull ) SWIFT_NOEXCEPT SWIFT_CALL; // takeGenericPair(_:)

// CHECK: template<class T_0_0, class T_0_1>
// CHECK-NEXT: requires swift::isUsableInGenericContext<T_0_0> && swift::isUsableInGenericContext<T_0_1>
// CHECK-NEXT: class GenericPair;

// CHECK: template<class T_0_0, class T_0_1>
// CHECK: template<class T_0_0, class T_0_1>
// CHECK-NEXT: requires swift::isUsableInGenericContext<T_0_0> && swift::isUsableInGenericContext<T_0_1>
// CHECK-NEXT: class GenericPair final {

// CHECK: swift::_impl::OpaqueStorage _storage;
// CHECK-NEXT: friend class _impl::_impl_GenericPair<T_0_0, T_0_1>;
// CHECK-NEXT: }

// CHECK: inline void inoutConcretePair(uint16_t x, GenericPair<uint16_t, uint16_t>& y) noexcept {
// CHECK-NEXT: return _impl::$s8Generics17inoutConcretePairyys6UInt16V_AA07GenericD0VyA2DGztF(x, _impl::_impl_GenericPair<uint16_t, uint16_t>::getOpaquePointer(y));


// CHECK: inline void inoutGenericPair(GenericPair<T_0_0, T_0_1>& x, const T_0_0& y) noexcept {
// CHECK-NEXT:   return _impl::$s8Generics16inoutGenericPairyyAA0cD0Vyxq_Gz_xtr0_lF(_impl::_impl_GenericPair<T_0_0, T_0_1>::getOpaquePointer(x), swift::_impl::getOpaquePointer(y), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata());


// CHECK: inline GenericPair<uint16_t, uint16_t> makeConcretePair(uint16_t x, uint16_t y) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK-NEXT:  return _impl::_impl_GenericPair<uint16_t, uint16_t>::returnNewValue([&](char * _Nonnull result) {
// CHECK-NEXT:    _impl::swift_interop_returnDirect_Generics_[[PTRPTRENC]](result, _impl::$s8Generics16makeConcretePairyAA07GenericD0Vys6UInt16VAFGAF_AFtF(x, y));

// CHECK: inline GenericPair<T_0_0, T_0_1> makeGenericPair(const T_0_0& x, const T_0_1& y) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK-NEXT:  return _impl::_impl_GenericPair<T_0_0, T_0_1>::returnNewValue([&](char * _Nonnull result) {
// CHECK-NEXT:    _impl::swift_interop_returnDirect_Generics_[[PTRPTRENC]](result, _impl::$s8Generics15makeGenericPairyAA0cD0Vyxq_Gx_q_tr0_lF(swift::_impl::getOpaquePointer(x), swift::_impl::getOpaquePointer(y), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata()));

// CHECK: inline GenericPair<uint16_t, uint16_t> passThroughConcretePair(const GenericPair<uint16_t, uint16_t>& x, uint16_t y) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK-NEXT:  return _impl::_impl_GenericPair<uint16_t, uint16_t>::returnNewValue([&](char * _Nonnull result) {
// CHECK-NEXT:    _impl::swift_interop_returnDirect_Generics_[[PTRPTRENC]](result, _impl::$s8Generics23passThroughConcretePair_1yAA07GenericE0Vys6UInt16VAGGAH_AGtF(_impl::swift_interop_passDirect_Generics_[[PTRPTRENC]](_impl::_impl_GenericPair<uint16_t, uint16_t>::getOpaquePointer(x)), y));

// CHECK: inline GenericPair<T_0_0, T_0_1> passThroughGenericPair(const GenericPair<T_0_0, T_0_1>& x, const T_0_1& y) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK-NEXT:  return _impl::_impl_GenericPair<T_0_0, T_0_1>::returnNewValue([&](char * _Nonnull result) {
// CHECK-NEXT:    _impl::swift_interop_returnDirect_Generics_[[PTRPTRENC]](result, _impl::$s8Generics22passThroughGenericPairyAA0dE0Vyxq_GAE_q_tr0_lF(_impl::swift_interop_passDirect_Generics_[[PTRPTRENC]](_impl::_impl_GenericPair<T_0_0, T_0_1>::getOpaquePointer(x)), swift::_impl::getOpaquePointer(y), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata()));

// CHECK: inline void takeConcretePair(const GenericPair<uint16_t, uint16_t>& x) noexcept {
// CHECK-NEXT:  return _impl::$s8Generics16takeConcretePairyyAA07GenericD0Vys6UInt16VAFGF(_impl::swift_interop_passDirect_Generics_[[PTRPTRENC]](_impl::_impl_GenericPair<uint16_t, uint16_t>::getOpaquePointer(x)));


// CHECK:  inline void takeGenericPair(const GenericPair<T_0_0, T_0_1>& x) noexcept {
// CHECK-NEXT:  return _impl::$s8Generics15takeGenericPairyyAA0cD0Vyxq_Gr0_lF(_impl::swift_interop_passDirect_Generics_[[PTRPTRENC]](_impl::_impl_GenericPair<T_0_0, T_0_1>::getOpaquePointer(x)), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata());

// CHECK: inline T_0_1 GenericPair<T_0_0, T_0_1>::getY() const {
// CHECK: _impl::$s8Generics11GenericPairV1yq_vg(reinterpret_cast<void *>(&returnValue), _impl::swift_interop_passDirect_Generics_[[PTRPTRENC]](_getOpaquePointer()), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata());

// CHECK: inline void GenericPair<T_0_0, T_0_1>::setY(const T_0_1& newValue) {
// CHECK-NEXT: return _impl::$s8Generics11GenericPairV1yq_vs(swift::_impl::getOpaquePointer(newValue), swift::TypeMetadataTrait<GenericPair<T_0_0, T_0_1>>::getTypeMetadata(), _getOpaquePointer());

// CHECK: inline GenericPair<T_0_0, T_0_1> GenericPair<T_0_0, T_0_1>::init(const T_0_0& x, swift::Int i, const T_0_1& y)
// CHECK-NEXT: return _impl::_impl_GenericPair<T_0_0, T_0_1>::returnNewValue([&](char * _Nonnull result) {
// CHECK-NEXT: _impl::swift_interop_returnDirect_Generics_[[PTRPTRENC]](result, _impl::$s8Generics11GenericPairVyACyxq_Gx_Siq_tcfC(swift::_impl::getOpaquePointer(x), i, swift::_impl::getOpaquePointer(y), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata()));

// CHECK: inline T_1_0 GenericPair<T_0_0, T_0_1>::genericMethod(const T_1_0& x, const T_0_1& y) const {
// CHECK: _impl::$s8Generics11GenericPairV13genericMethodyqd__qd___q_tlF(reinterpret_cast<void *>(&returnValue), swift::_impl::getOpaquePointer(x), swift::_impl::getOpaquePointer(y), _impl::swift_interop_passDirect_Generics_[[PTRPTRENC]](_getOpaquePointer()), swift::TypeMetadataTrait<T_0_0>::getTypeMetadata(), swift::TypeMetadataTrait<T_0_1>::getTypeMetadata(), swift::TypeMetadataTrait<T_1_0>::getTypeMetadata());
