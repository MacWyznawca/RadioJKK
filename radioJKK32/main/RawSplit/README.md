
# Raw split audio element for ESP-ADF

Element for branching audio stream to additional pipelines anywhere in the main pipeline

## Brief of use

If you need to branch the audio stream, e.g. to simultaneously play audio from the source to output and save it to the SD card, use this element.

The element receives audio from the previous one and passes it to the next one, at the same time it allows you to add branches to more pipelines Their number is specified in the configuration parameter `multi_out_num`.

ESP-ADF has provided for such branching only in the http_stream and i2s_stream elements. However, it is often convenient to branch elsewhere, for example, after decoding the audio stream.

```
HTTP server > http_stream_reader --> decoder --> raw_split --> equalizer --> i2s_stream > codec chip
                                                    |
                                                    v
                                                     --> raw_stream --> aac_encoder --> fatfs_stream > SD card
```

For recieve audio data from `raw_split` use `raw_stream` element.

```c
    raw_split_cfg_t rs_cfg = RAW_SPLIT_CFG_DEFAULT();
    raw_split.multi_out_num = 1;
    audio_element_handle_t raw_split = raw_split_init(&rs_cfg);

//
//

    ringbuf_handle_t rb = audio_element_get_output_ringbuf(raw_stream);
    audio_element_set_multi_output_ringbuf(raw_split, rb, 0);
```

For more branches set `multi_out_num` for number fo branches

```
HTTP server > http_stream_reader --> decoder --> raw_split --> equalizer --> i2s_stream > Codec chip
                                                    |
                                                    v
                                                     --> raw_stream --> aac_encoder --> fatfs_stream > SD card
                                                    |
                                                    v
                                                     --> raw_stream_bt --> resample_for_bt --> bt_stream_writer > Bluetooth speaker
```
Smaple code for more branches:
```c
    raw_split_cfg_t rs_cfg = RAW_SPLIT_CFG_DEFAULT();
    raw_split.multi_out_num = 2;
    audio_element_handle_t raw_split = raw_split_init(&rs_cfg);

//
//

    ringbuf_handle_t rb0 = audio_element_get_output_ringbuf(raw_stream);
    ringbuf_handle_t rb1 = audio_element_get_output_ringbuf(raw_stream_bt);
    audio_element_set_multi_output_ringbuf(raw_split, rb0, 0);
    audio_element_set_multi_output_ringbuf(raw_split, rb1, 1);
```
