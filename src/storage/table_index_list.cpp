#include "duckdb/storage/table/table_index_list.hpp"
#include "duckdb/storage/data_table.hpp"

namespace duckdb {
void TableIndexList::AddIndex(unique_ptr<Index> index) {
	D_ASSERT(index);
	lock_guard<mutex> lock(indexes_lock);
	indexes.push_back(move(index));
}
void TableIndexList::RemoveIndex(Index *index) {
	D_ASSERT(index);
	lock_guard<mutex> lock(indexes_lock);

	for (idx_t index_idx = 0; index_idx < indexes.size(); index_idx++) {
		auto &index_entry = indexes[index_idx];
		if (index_entry.get() == index) {
			indexes.erase(indexes.begin() + index_idx);
			break;
		}
	}
}

bool TableIndexList::Empty() {
	lock_guard<mutex> lock(indexes_lock);
	return indexes.empty();
}

idx_t TableIndexList::Count() {
	lock_guard<mutex> lock(indexes_lock);
	return indexes.size();
}

void TableIndexList::Move(TableIndexList &other) {
	D_ASSERT(indexes.empty());
	indexes = move(other.indexes);
}

Index *TableIndexList::FindForeignKeyIndex(const vector<idx_t> &fk_keys, ForeignKeyType fk_type) {
	Index *result = nullptr;
	Scan([&](Index &index) {
		if (DataTable::IsForeignKeyIndex(fk_keys, index, fk_type)) {
			result = &index;
		}
		return false;
	});
	return result;
}

void TableIndexList::VerifyForeignKey(const vector<idx_t> &fk_keys, bool is_append, DataChunk &chunk,
                                      vector<string> &err_msgs) {
	auto fk_type = is_append ? ForeignKeyType::FK_TYPE_PRIMARY_KEY_TABLE : ForeignKeyType::FK_TYPE_FOREIGN_KEY_TABLE;

	// check whether or not the chunk can be inserted or deleted into the referenced table' storage
	auto index = FindForeignKeyIndex(fk_keys, fk_type);
	if (!index) {
		throw InternalException("Internal Foreign Key error: could not find index to verify...");
	}
	if (is_append) {
		index->VerifyAppendForeignKey(chunk, err_msgs.data());
	} else {
		index->VerifyDeleteForeignKey(chunk, err_msgs.data());
	}
}

vector<BlockPointer> TableIndexList::SerializeIndexes(duckdb::MetaBlockWriter &writer) {
	vector<BlockPointer> blocks_info;
	for (auto &index : indexes) {
		blocks_info.emplace_back(index->Serialize(writer));
	}
	return blocks_info;
}

} // namespace duckdb
