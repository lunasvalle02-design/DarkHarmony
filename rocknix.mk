# Castlevania: Harmony of Dissonance DS — ROCKNIX build (Linux / WSL / macOS hosts).
#   make -f rocknix.mk image all deploy DEVICE_IP=192.168.1.50
IMAGE ?= hod-ds-builder:bookworm
RUN = docker run --rm -t -v "$(CURDIR):/work" -w /work \
	-e DEVICE_IP=$(DEVICE_IP) -e DEVICE_USER=$(or $(DEVICE_USER),root) -e DEVICE_PASS=$(DEVICE_PASS) \
	-e JOBS=$(JOBS) -e GUEST_OPT=$(GUEST_OPT) -e INCLUDE_GAME_FILES=$(INCLUDE_GAME_FILES) $(IMAGE)

.PHONY: image fetch build package deploy shell all clean
image:   ; docker build -t $(IMAGE) rocknix/docker
fetch:   ; $(RUN) sh rocknix/scripts/fetch-sources.sh
build:   ; $(RUN) sh rocknix/scripts/build.sh
package: ; $(RUN) sh rocknix/scripts/package.sh
deploy:  ; $(RUN) sh rocknix/scripts/deploy.sh
shell:   ; docker run --rm -it -v "$(CURDIR):/work" -w /work $(IMAGE) bash
all: fetch build package
clean:   ; rm -rf build/rocknix dist
