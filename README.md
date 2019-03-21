# Auto Comment Generation

This script implement basic doxygen style annotation check for files and their methods. If the origin code don't have correct annotation it can auto generate an annotation pattern. It can also check and add file header.

## Example Input 

One file name each line in file_list.txt.

```
examples/encode_hevc_vdenc_packet_g12.h
examples/encode_hevc_vdenc_packet_g12.cpp
examples/decode_hevc_pipeline.h
```



## Output

New checked files with test_  add to the original file name in the same directory.

```
examples/test_encode_hevc_vdenc_packet_g12.h
examples/test_encode_hevc_vdenc_packet_g12.cpp
examples/test_decode_hevc_pipeline.h
```

