if (!('wasmTable' in Module)) {
    Module.wasmTable = wasmTable
}

Module.FS = FS
Module.PATH = PATH
Module.LDSO = LDSO
Module.getDylinkMetadata = getDylinkMetadata
Module.loadDynamicLibrary = loadDynamicLibrary

Module.UTF8ToString = UTF8ToString;
Module.ERRNO_CODES = ERRNO_CODES;