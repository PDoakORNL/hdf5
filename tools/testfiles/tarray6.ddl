#############################
Expected output for 'h5dump tarray6.h5'
#############################
HDF5 "tarray6.h5" {
GROUP "/" {
   DATASET "Dataset1" {
      DATATYPE  H5T_ARRAY { [4] H5T_VLEN { H5T_STD_U32LE} }
      DATASPACE  SIMPLE { ( 4 ) / ( 4 ) }
      DATA {
        (0) [ (0), (10, 11), (20, 21, 22), (30, 31, 32, 33) ],
        (1) [ (100, 101), (110, 111, 112), (120, 121, 122, 123), (130, 131, 132, 133, 134) ],
        (2) [ (200, 201, 202), (210, 211, 212, 213), (220, 221, 222, 223, 224), (230, 231, 232, 233, 234, 235) ],
        (3) [ (300, 301, 302, 303), (310, 311, 312, 313, 314), (320, 321, 322, 323, 324, 325), (330, 331, 332, 333, 334, 335, 336) ]
      }
   }
}
}
