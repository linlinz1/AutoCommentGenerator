# Auto Comment Generation

This script implement basic doxygen style annotation check for files and their methods. If the origin code don't have correct annotation it can auto generate an annotation pattern. It can also check and add file header.

## Usage 

1. Input the target files' names in file_list.txt and one file name each line like following.

```
examples/encode_hevc_vdenc_packet_g12.h
examples/encode_hevc_vdenc_packet_g12.cpp
examples/decode_hevc_pipeline.h
```

2. Then run the scripts.

```
python auto_comment_gen.py
```

3. New commented files will be generated in the same directory with test_  add to the original file name .

```
examples/test_encode_hevc_vdenc_packet_g12.h
examples/test_encode_hevc_vdenc_packet_g12.cpp
examples/test_decode_hevc_pipeline.h
```

