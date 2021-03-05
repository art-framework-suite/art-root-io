if (ART_ROOT_IO)
  list(PREPEND ART_ROOT_IO
      art_root_io::tfile_support
  )
endif()