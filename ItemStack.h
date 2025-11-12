#pragma once
#include "Block.h"

struct ItemStack {
	BlockID id = BlockID::Air;
	int count = 0;
	ItemStack() = default;
	ItemStack(BlockID id, int count) : id(id), count(count) {}

	bool isEmpty() const {
		return id == BlockID::Air || count <= 0;
	}

	void clear() {
		id = BlockID::Air;
		count = 0;
	}
};