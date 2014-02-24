{
    'variables': {
        'wtf_files': [
            'ASCIICType.h',
            'Alignment.h',
            'ArrayBuffer.cpp',
            'ArrayBuffer.h',
            'ArrayBufferBuilder.cpp',
            'ArrayBufferBuilder.h',
            'ArrayBufferContents.cpp',
            'ArrayBufferContents.h',
            'ArrayBufferDeallocationObserver.h',
            'ArrayBufferView.cpp',
            'ArrayBufferView.h',
            'Assertions.cpp',
            'Assertions.h',
            'Atomics.h',
            'AutodrainedPool.h',
            'AutodrainedPoolMac.mm',
            'BitArray.h',
            'BitVector.cpp',
            'BitVector.h',
            'BloomFilter.h',
            'ByteOrder.h',
            'ByteSwap.h',
            'CPU.h',
            'CheckedArithmetic.h',
            'Compiler.h',
            'Complex.h',
            'CryptographicallyRandomNumber.cpp',
            'CryptographicallyRandomNumber.h',
            'CurrentTime.cpp',
            'CurrentTime.h',
            'DataLog.cpp',
            'DataLog.h',
            'DateMath.cpp',
            'DateMath.h',
            'DecimalNumber.cpp',
            'DecimalNumber.h',
            'Deque.h',
            'DoublyLinkedList.h',
            'DynamicAnnotations.cpp',
            'DynamicAnnotations.h',
            'FastAllocBase.h',
            'FastMalloc.cpp',
            'FastMalloc.h',
            'FilePrintStream.cpp',
            'FilePrintStream.h',
            'Float32Array.h',
            'Float64Array.h',
            'Forward.h',
            'Functional.h',
            'GetPtr.h',
            'GregorianDateTime.cpp',
            'GregorianDateTime.h',
            'HashCountedSet.h',
            'HashFunctions.h',
            'HashIterators.h',
            'HashMap.h',
            'HashSet.h',
            'HashTable.cpp',
            'HashTable.h',
            'HashTableDeletedValueType.h',
            'HashTraits.h',
            'HexNumber.h',
            'Int16Array.h',
            'Int32Array.h',
            'Int8Array.h',
            'IntegralTypedArrayBase.h',
            'LeakAnnotations.h',
            'LinkedStack.h',
            'ListHashSet.h',
            'Locker.h',
            'MainThread.cpp',
            'MainThread.h',
            'MallocZoneSupport.h',
            'MathExtras.h',
            'MessageQueue.h',
            'NonCopyingSort.h',
            'Noncopyable.h',
            'NotFound.h',
            'NullPtr.cpp',
            'NullPtr.h',
            'NumberOfCores.cpp',
            'NumberOfCores.h',
            'OwnPtr.h',
            'OwnPtrCommon.h',
            'PageAllocator.cpp',
            'PageAllocator.h',
            'ParallelJobs.h',
            'ParallelJobsGeneric.cpp',
            'ParallelJobsGeneric.h',
            'ParallelJobsLibdispatch.h',
            'PartitionAlloc.cpp',
            'PartitionAlloc.h',
            'PassOwnPtr.h',
            'PassRefPtr.h',
            'PassTraits.h',
            'PrintStream.cpp',
            'PrintStream.h',
            'ProcessID.h',
            'QuantizedAllocation.cpp',
            'QuantizedAllocation.h',
            'RefCounted.h',
            'RefCountedLeakCounter.cpp',
            'RefCountedLeakCounter.h',
            'RefPtr.h',
            'RefPtrHashMap.h',
            'RetainPtr.h',
            'SHA1.cpp',
            'SHA1.h',
            'SaturatedArithmetic.h',
            'SizeLimits.cpp',
            'SpinLock.h',
            'StaticConstructors.h',
            'StdLibExtras.h',
            'StringExtras.h',
            'StringHasher.h',
            'TCPackedCache.h',
            'TCPageMap.h',
            'TCSpinLock.h',
            'TCSystemAlloc.cpp',
            'TCSystemAlloc.h',
            'TemporaryChange.h',
            'ThreadFunctionInvocation.h',
            'ThreadIdentifierDataPthreads.cpp',
            'ThreadIdentifierDataPthreads.h',
            'ThreadRestrictionVerifier.h',
            'ThreadSafeRefCounted.h',
            'ThreadSpecific.h',
            'ThreadSpecificWin.cpp',
            'Threading.cpp',
            'Threading.h',
            'ThreadingPrimitives.h',
            'ThreadingPthreads.cpp',
            'ThreadingWin.cpp',
            'TypeTraits.cpp',
            'TypeTraits.h',
            'TypedArrayBase.h',
            'Uint16Array.h',
            'Uint32Array.h',
            'Uint8Array.h',
            'UnusedParam.h',
            'VMTags.h',
            'Vector.h',
            'VectorTraits.h',
            'WTF.cpp',
            'WTF.h',
            'WTFExport.h',
            'WTFThreadData.cpp',
            'WTFThreadData.h',
            'WeakPtr.h',
            'dtoa.cpp',
            'dtoa.h',
            'dtoa/bignum-dtoa.cc',
            'dtoa/bignum-dtoa.h',
            'dtoa/bignum.cc',
            'dtoa/bignum.h',
            'dtoa/cached-powers.cc',
            'dtoa/cached-powers.h',
            'dtoa/diy-fp.cc',
            'dtoa/diy-fp.h',
            'dtoa/double-conversion.cc',
            'dtoa/double-conversion.h',
            'dtoa/double.h',
            'dtoa/fast-dtoa.cc',
            'dtoa/fast-dtoa.h',
            'dtoa/fixed-dtoa.cc',
            'dtoa/fixed-dtoa.h',
            'dtoa/strtod.cc',
            'dtoa/strtod.h',
            'dtoa/utils.h',
            'text/ASCIIFastPath.h',
            'text/AtomicString.cpp',
            'text/AtomicString.h',
            'text/AtomicStringCF.cpp',
            'text/AtomicStringHash.h',
            'text/Base64.cpp',
            'text/Base64.h',
            'text/CString.cpp',
            'text/CString.h',
            'text/IntegerToStringConversion.h',
            'text/StringBuffer.h',
            'text/StringBuilder.cpp',
            'text/StringBuilder.h',
            'text/StringCF.cpp',
            'text/StringConcatenate.h',
            'text/StringHash.h',
            'text/StringImpl.cpp',
            'text/StringImpl.h',
            'text/StringImplCF.cpp',
            'text/StringImplMac.mm',
            'text/StringMac.mm',
            'text/StringOperators.h',
            'text/StringStatics.cpp',
            'text/StringUTF8Adaptor.h',
            'text/StringView.h',
            'text/TextCodec.cpp',
            'text/TextCodecASCIIFastPath.h',
            'text/TextCodecICU.cpp',
            'text/TextCodecLatin1.cpp',
            'text/TextCodecUTF16.cpp',
            'text/TextCodecUTF8.cpp',
            'text/TextCodecUTF8.h',
            'text/TextCodecUserDefined.cpp',
            'text/TextEncoding.cpp',
            'text/TextEncodingRegistry.cpp',
            'text/TextPosition.cpp',
            'text/TextPosition.h',
            'text/WTFString.cpp',
            'text/WTFString.h',
            'unicode/CharacterNames.h',
            'unicode/Collator.h',
            'unicode/UTF8.cpp',
            'unicode/UTF8.h',
            'unicode/Unicode.h',
            'unicode/icu/CollatorICU.cpp',
            'unicode/icu/UnicodeIcu.h',
        ],
        'wtf_unittest_files': [
            'ArrayBufferBuilderTest.cpp',
            'CheckedArithmeticTest.cpp',
            'FunctionalTest.cpp',
            'HashMapTest.cpp',
            'HashSetTest.cpp',
            'ListHashSetTest.cpp',
            'MathExtrasTest.cpp',
            'PartitionAllocTest.cpp',
            'SHA1Test.cpp',
            'SaturatedArithmeticTest.cpp',
            'SpinLockTest.cpp',
            'StringExtrasTest.cpp',
            'StringHasherTest.cpp',
            'TemporaryChangeTest.cpp',
            'VectorTest.cpp',
            'testing/WTFTestHelpers.h',
            'text/CStringTest.cpp',
            'text/StringBuilderTest.cpp',
            'text/StringImplTest.cpp',
            'text/StringOperatorsTest.cpp',
            'text/TextCodecUTF8Test.cpp',
            'text/WTFStringTest.cpp',
        ],
    },
}
