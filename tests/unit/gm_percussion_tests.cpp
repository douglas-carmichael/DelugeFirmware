#include "CppUTest/TestHarness.h"
#include "model/drum/gm_percussion.h"
#include <cstring>

TEST_GROUP(GMPercussionTests){};

TEST(GMPercussionTests, notesOutsideTheKeyMapHaveNoName) {
	// The caller uses nullptr to decide whether to fall back to the bare note name, so the boundaries matter.
	POINTERS_EQUAL(nullptr, deluge::gm::percussionName(deluge::gm::kFirstPercussionNote - 1));
	POINTERS_EQUAL(nullptr, deluge::gm::percussionName(deluge::gm::kLastPercussionNote + 1));
	POINTERS_EQUAL(nullptr, deluge::gm::percussionName(0));
	POINTERS_EQUAL(nullptr, deluge::gm::percussionName(127));
}

TEST(GMPercussionTests, keyMapEndsAreNamed) {
	STRCMP_EQUAL("Acoustic Bass Drum", deluge::gm::percussionName(deluge::gm::kFirstPercussionNote));
	STRCMP_EQUAL("Open Triangle", deluge::gm::percussionName(deluge::gm::kLastPercussionNote));
}

TEST(GMPercussionTests, tableIsNotOffByOne) {
	// Spot-check the notes people actually recognise. An off-by-one in the table would mislabel every row.
	STRCMP_EQUAL("Bass Drum 1", deluge::gm::percussionName(36));
	STRCMP_EQUAL("Acoustic Snare", deluge::gm::percussionName(38));
	STRCMP_EQUAL("Closed Hi-Hat", deluge::gm::percussionName(42));
	STRCMP_EQUAL("Open Hi-Hat", deluge::gm::percussionName(46));
	STRCMP_EQUAL("Crash Cymbal 1", deluge::gm::percussionName(49));
	STRCMP_EQUAL("Ride Cymbal 2", deluge::gm::percussionName(59));
}

TEST(GMPercussionTests, everyNoteInTheKeyMapIsNamed) {
	for (int32_t note = deluge::gm::kFirstPercussionNote; note <= deluge::gm::kLastPercussionNote; note++) {
		char const* name = deluge::gm::percussionName(note);
		CHECK(name != nullptr);
		CHECK(std::strlen(name) > 0);
	}
}

TEST(GMPercussionTests, drumKitBlockSitsInsideTheKeyMap) {
	// The GM fill walks this range and shows each row's name, so it must not run off either end of the table.
	CHECK(deluge::gm::kFirstDrumKitNote >= deluge::gm::kFirstPercussionNote);
	CHECK(deluge::gm::kLastDrumKitNote <= deluge::gm::kLastPercussionNote);
	CHECK(deluge::gm::kFirstDrumKitNote < deluge::gm::kLastDrumKitNote);
}
