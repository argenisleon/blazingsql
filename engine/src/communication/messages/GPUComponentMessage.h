#pragma once

#include <blazingdb/transport/Address.h>
#include <blazingdb/transport/ColumnTransport.h>
#include <blazingdb/transport/Message.h>
#include <blazingdb/transport/Node.h>

#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <tuple>

#include <cudf/copying.hpp>
#include <cudf/column/column.hpp>
#include <cudf/column/column_view.hpp>
#include <cudf/null_mask.hpp>
#include <cudf/types.hpp>
#include <execution_graph/logic_controllers/LogicPrimitives.h>
#include "Traits/RuntimeTraits.h"

#include "GDFColumn.cuh"

#include "from_cudf/cpp_tests/utilities/column_utilities.hpp"

namespace ral {
namespace communication {
namespace messages {

using Node = blazingdb::transport::Node;
using Address = blazingdb::transport::Address;
using ColumnTransport = blazingdb::transport::ColumnTransport;
using GPUMessage = blazingdb::transport::GPUMessage;
using StrDataPtrTuple = std::tuple<char *, cudf::size_type, int *, cudf::size_type, unsigned char *, cudf::size_type>;

inline bool isGdfString(const cudf::column_view& column) {
	cudf::type_id col_type = column.type().id();
	bool ret = (cudf::type_id::STRING == col_type || cudf::type_id::CATEGORY == col_type);
	return ret;
}

inline cudf::size_type getValidCapacity(const cudf::column_view & column) {
	return column.null_count() > 0 ? cudf::bitmask_allocation_size_bytes(column.size()) : 0;
}



class GPUReceivedMessage : public GPUMessage {
public:

	GPUReceivedMessage(std::string const & messageToken,
						uint32_t contextToken,
						std::shared_ptr<Node> & sender_node,
					    std::unique_ptr<ral::frame::BlazingTable> && samples,
						std::uint64_t total_row_size = 0)
		: GPUMessage(messageToken, contextToken, sender_node),
		  table(std::move(samples)) {
		this->metadata().total_row_size = total_row_size;
	}
	virtual raw_buffer GetRawColumns() {
		assert(false);
	}

	CudfTableView getSamples() { return table->view(); }

	// ral::frame::BlazingTable release() { return std::move(table); }

protected:
	std::unique_ptr<ral::frame::BlazingTable> table;

	std::map<cudf::column *, StrDataPtrTuple> strDataColToPtrMap;

	std::mutex strigDataMutex;
};

class GPUComponentMessage : public GPUMessage {
public:
	GPUComponentMessage(std::string const & messageToken,
		uint32_t contextToken,
		std::shared_ptr<Node> & sender_node,
		std::unique_ptr<ral::frame::BlazingTableView> && samples,
		std::uint64_t total_row_size = 0)
		: GPUMessage(messageToken, contextToken, sender_node), table_view{std::move(samples)} {
		this->metadata().total_row_size = total_row_size;
	}


	// ~GPUComponentMessage() {
	// 	for(auto && e : strDataColToPtrMap) {
	// 		char * stringsPointer;
	// 		int * offsetsPointer;
	// 		unsigned char * nullBitmask;
	// 		std::tie(stringsPointer, std::ignore, offsetsPointer, std::ignore, nullBitmask, std::ignore) = e.second;
	// 		RMM_TRY(RMM_FREE(stringsPointer, 0));
	// 		RMM_TRY(RMM_FREE(offsetsPointer, 0));
	// 		RMM_TRY(RMM_FREE(nullBitmask, 0));
	// 	}
	// }

// 	auto get_raw_pointers(cudf::column * column) {
// 		std::lock_guard<std::mutex> lock(strigDataMutex);

// 		// TODO percy cudf0.12 custrings this was not commented
// //		auto strDataTupleIt = strDataColToPtrMap.find(column);
// //		if(strDataTupleIt != strDataColToPtrMap.end()) {
// //			return strDataTupleIt->second;
// //		}
// //		bool deleteCategory = false;
// //		void * category_ptr = column->dtype_info.category;
// //		if(!category_ptr) {
// //			category_ptr = NVCategory::create_from_array(nullptr, 0);
// //			deleteCategory = true;
// //		}
		
// 		auto size = column->size();
// 		auto null_count = column->null_count();
// 		std::string output;
		
// 		// TODO percy cudf0.12 custrings this was not commented
// //		NVCategory * category = static_cast<NVCategory *>(category_ptr);
// //		NVStrings * nvStrings_ = category->gather_strings(category->values_cptr(), category->size(), true);
// //		cudf::size_type stringsLength_ = nvStrings_->size();
// //		auto offsetsLength_ = stringsLength_ + 1;

// //		int * const lengthPerStrings = new int[stringsLength_];
// //		nvStrings_->byte_count(lengthPerStrings, false);
// //		cudf::size_type stringsSize_ =
// //			std::accumulate(lengthPerStrings,
// //				lengthPerStrings + stringsLength_,
// //				0,
// //				[](int accumulator, int currentValue) { return accumulator + std::max(currentValue, 0); }) *
// //			sizeof(char);
// //		char * stringsPointer_ = nullptr;
// //		cudf::size_type offsetsSize_ = offsetsLength_ * sizeof(int);
// //		int * offsetsPointer_ = nullptr;
// //		RMM_TRY(RMM_ALLOC(reinterpret_cast<void **>(&stringsPointer_), stringsSize_, 0));
// //		RMM_TRY(RMM_ALLOC(reinterpret_cast<void **>(&offsetsPointer_), offsetsSize_, 0));

// //		cudf::size_type nullMaskSize_ = 0;
// //		unsigned char * nullBitmask_ = nullptr;
// //		if(null_count > 0) {
// //			nullMaskSize_ = ral::traits::get_bitmask_size_in_bytes(stringsLength_);
// //			RMM_TRY(RMM_ALLOC(reinterpret_cast<void **>(&nullBitmask_), nullMaskSize_, 0));
// //		}
// //		nvStrings_->create_offsets(stringsPointer_, offsetsPointer_, nullBitmask_, true);

// //		delete[] lengthPerStrings;
// //		NVStrings::destroy(nvStrings_);
// //		if(deleteCategory) {
// //			NVCategory::destroy(category);
// //		}

// //		auto strDataTuple =
// //			std::make_tuple(stringsPointer_, stringsSize_, offsetsPointer_, offsetsSize_, nullBitmask_, nullMaskSize_);
// //		strDataColToPtrMap[column] = strDataTuple;

// //		return strDataTuple;
// 	}

	virtual raw_buffer GetRawColumns() override {
		std::vector<int> buffer_sizes;
		std::vector<const char *> raw_buffers;
		std::vector<ColumnTransport> column_offset;
		for(int i = 0; i < table_view->num_columns(); ++i) {
			const cudf::column_view&column = table_view->column(i);
			ColumnTransport col_transport = ColumnTransport{ColumnTransport::MetaData{
																.dtype = (int32_t)column.type().id(),
																.size = column.size(),
																.null_count = column.null_count(),
																.col_name = {},
															},
				.data = -1,
				.valid = -1,
				.strings_data = -1,
				.strings_offsets = -1,
				.strings_nullmask = -1};
			strcpy(col_transport.metadata.col_name, table_view->names().at(i).c_str());
			if(isGdfString(column)) {
				auto num_children = column.num_children();
				assert(num_children == 2);
				auto offsets_column = column.child(0);
				auto chars_column = column.child(1);

			 	col_transport.strings_data = raw_buffers.size();
				buffer_sizes.push_back(chars_column.size());
				raw_buffers.push_back(chars_column.data<char>());

				col_transport.strings_offsets = raw_buffers.size();
				buffer_sizes.push_back(offsets_column.size());
				raw_buffers.push_back(offsets_column.data<char>());

				if(nullBitmask != nullptr) {
					col_transport.strings_nullmask = raw_buffers.size();
					buffer_sizes.push_back(getValidCapacity(column));
					raw_buffers.push_back(column.null_mask());
				}
			} else if(column.has_nulls() and column.null_count() > 0) {
				// case: valid
				col_transport.data = raw_buffers.size();
				buffer_sizes.push_back(column.size() * cudf::size_of(column.type()));
				raw_buffers.push_back(column.data<char>());
				col_transport.valid = raw_buffers.size();
				buffer_sizes.push_back(getValidCapacity(column));
				raw_buffers.push_back(column.null_mask());
			} else {
				// case: data
				col_transport.data = raw_buffers.size();
				buffer_sizes.push_back(column.size() * cudf::size_of(column.type()));
				raw_buffers.push_back(column.data<char>());
			}
			column_offset.push_back(col_transport);
		}
		return std::make_tuple(buffer_sizes, raw_buffers, column_offset);
	}

	static std::shared_ptr<GPUMessage> MakeFrom(const Message::MetaData & message_metadata,
		const Address::MetaData & address_metadata,
		const std::vector<ColumnTransport> & columns_offsets,
		const std::vector<const char *> & raw_buffers) {  // gpu pointer
		auto node = std::make_shared<Node>(
			Address::TCP(address_metadata.ip, address_metadata.comunication_port, address_metadata.protocol_port));
		auto num_columns = columns_offsets.size();
		std::vector<std::unique_ptr<cudf::column>> received_samples(num_columns);
		std::vector<std::string> column_names(num_columns);

		assert(raw_buffers.size() >= 0);
		for(size_t i = 0; i < num_columns; ++i) {
			auto column = new cudf::column();
			auto data_offset = columns_offsets[i].data;
			auto string_offset = columns_offsets[i].strings_data;
			if(string_offset != -1) {
				// // this is a string
				// // NVCategory *nvcategory_ptr = nullptr;
				// // auto string_offset = columns_offsets[i].strings_data;
				// // nvcategory_ptr = (NVCategory *) raw_buffers[string_offset];
				char * charsPointer = (char *) raw_buffers[columns_offsets[i].strings_data];
				int * offsetsPointer = (int *) raw_buffers[columns_offsets[i].strings_offsets];
				unsigned char * nullMaskPointer = nullptr;
				if(columns_offsets[i].strings_nullmask != -1) {
					nullMaskPointer = (unsigned char *) raw_buffers[columns_offsets[i].strings_nullmask];
				}

				// auto nvcategory_ptr = NVCategory::create_from_offsets(reinterpret_cast<const char *>(stringsPointer),
				// 	columns_offsets[i].metadata.size,
				// 	reinterpret_cast<const int *>(offsetsPointer),
				// 	reinterpret_cast<const unsigned char *>(nullMaskPointer),
				// 	columns_offsets[i].metadata.null_count,
				// 	true);
				// received_samples[i].create_gdf_column(
				// 	nvcategory_ptr, columns_offsets[i].metadata.size, (char *) columns_offsets[i].metadata.col_name);

				// RMM_TRY(RMM_FREE(stringsPointer, 0));
				// RMM_TRY(RMM_FREE(offsetsPointer, 0));
				// RMM_TRY(RMM_FREE(nullMaskPointer, 0));

				// auto unique_column = std::make_unique<cudf::column>(dtype, column_size, data_buff, valid_buff);


			} else {
				cudf::data_type dtype = cudf::data_type{cudf::type_id(columns_offsets[i].metadata.dtype)};
				cudf::size_type column_size  =  (cudf::size_type)columns_offsets[i].metadata.size;

				cudf::valid_type * valid_ptr = nullptr;
				cudf::size_type valid_size = 0;
				if(columns_offsets[i].valid != -1) {
					// this is a valid
					auto valid_offset = columns_offsets[i].valid;
					valid_ptr = (cudf::valid_type *) raw_buffers[valid_offset];
					valid_size = cudf::bitmask_allocation_size_bytes(column_size);
				}
				assert(raw_buffers[data_offset] != nullptr);
				try {
					auto dtype_size = cudf::size_of(dtype);
					auto buffer_size = column_size * dtype_size;
					rmm::device_buffer data_buff(raw_buffers[data_offset], buffer_size);
					rmm::device_buffer valid_buff(valid_ptr, valid_size);
					auto unique_column = std::make_unique<cudf::column>(dtype, column_size, data_buff, valid_buff);
					received_samples[i] = std::move(unique_column);
				} catch(std::exception& e) {
					std::cerr << e.what() << std::endl;
				}
			}
			column_names[i] = std::string{columns_offsets[i].metadata.col_name};
		}
		auto unique_table = std::make_unique<cudf::experimental::table>(std::move(received_samples));
		auto received_table = std::make_unique<ral::frame::BlazingTable>(std::move(unique_table), column_names);
		return std::make_shared<GPUReceivedMessage>(message_metadata.messageToken,
			message_metadata.contextToken,
			node,
			std::move(received_table),
			message_metadata.total_row_size);
	}

	std::vector<gdf_column_cpp>  getSamples() { return std::vector<gdf_column_cpp>(); }

	std::vector<gdf_column_cpp>  getColumns() { return std::vector<gdf_column_cpp>(); }

	// ral::frame::BlazingTable release() { return std::move(table); }

protected:
	std::unique_ptr<ral::frame::BlazingTableView> table_view;

	std::map<cudf::column *, StrDataPtrTuple> strDataColToPtrMap;

	std::mutex strigDataMutex;
};

}  // namespace messages
}  // namespace communication
}  // namespace ral
