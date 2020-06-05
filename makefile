all:	decoder encoder
	@echo "Compile APP"

clean:
	rm decoder encoder

decoder: decoder.c
	@echo "Updata decoder"
	@gcc decoder.c -g -o decoder `pkg-config --cflags --libs libavformat libavcodec libavutil libswresample libswscale` -lSDL2

encoder: encoder.c
	@echo "Updata encoder"
	@gcc encoder.c -g -o encoder `pkg-config --cflags --libs libavformat libavcodec libavutil libswresample`

.PHONY: all clean clean
