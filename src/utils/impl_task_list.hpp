#pragma once

#include <stack>
#include <daxa/utils/task_list.hpp>

#define DAXA_TASKLIST_MAX_CONITIONALS 31

namespace daxa
{
    struct ImplDevice;

    using TaskBatchId = usize;

    using TaskId = usize;

    struct LastReadSplitBarrierIndex
    {
        usize index;
    };

    struct LastReadBarrierIndex
    {
        usize index;
    };

    struct PerPermTaskBuffer
    {
        /// Every permutation always has all buffers but they are not necessarily valid in that permutation.
        /// This boolean is used to check this.
        bool valid = {};
        BufferId transient_buffer_id = {};
        Access latest_access = AccessConsts::NONE;
        usize latest_access_batch_index = {};
        usize latest_access_submit_scope_index = {};
        usize first_access_batch_index = {};
        usize first_access_submit_scope_index = {};
        Access first_access = AccessConsts::NONE;
        // When the last index was a read and an additional read is followed after,
        // we will combine all barriers into one, which is the first barrier that the first read generates.
        std::variant<std::monostate, LastReadSplitBarrierIndex, LastReadBarrierIndex> latest_access_read_barrier_index = std::monostate{};
    };

    struct TaskBarrier
    {
        // when this ID is invalid, this barrier is NOT an image memory barrier but just a memory barrier.
        // So when ID invalid => memory barrier, ID valid => image memory barrier.
        TaskImageId image_id = {};
        ImageMipArraySlice slice = {};
        ImageLayout layout_before = {};
        ImageLayout layout_after = {};
        Access src_access = {};
        Access dst_access = {};
        usize src_batch = {};
        usize dst_batch = {};
    };

    struct TaskSplitBarrier : TaskBarrier
    {
        SplitBarrierState split_barrier_state;
    };

    struct ExtendedImageSliceState
    {
        ImageSliceState state = {};
        usize latest_access_batch_index = {};
        usize latest_access_submit_scope_index = {};
        // When the last index was a read and an additional read is followed after,
        // we will combine all barriers into one, which is the first barrier that the first read generates.
        std::variant<std::monostate, LastReadSplitBarrierIndex, LastReadBarrierIndex> latest_access_read_barrier_index = std::monostate{};
    };

    struct PerPermTaskImage
    {
        /// Every permutation always has all buffers but they are not necessarily valid in that permutation.
        /// This boolean is used to check this.
        bool valid = {};
        bool swapchain_semaphore_waited_upon = {};
        std::vector<ExtendedImageSliceState> last_slice_states = {};
        std::vector<ExtendedImageSliceState> first_slice_states = {};
        // only for transient images
        ImageUsageFlags usage = ImageUsageFlagBits::NONE;
        ImageId actual_image = {};
    };

    using ShaderUseIdToOffsetTable = std::vector<std::variant<std::pair<TaskImageId, usize>, std::pair<TaskBufferId, usize>, std::monostate>>;

    struct Task
    {
        TaskInfo info = {};
        std::vector<std::vector<ImageViewId>> image_view_cache = {};
        ShaderUseIdToOffsetTable id_to_offset = {};
        std::unordered_map<u32, u32> buffer_alias_hash_to_use_index = {};
        std::unordered_map<u32, u32> image_alias_hash_to_use_index = {};
    };

    struct CreateTaskBufferTask
    {
        TaskBufferId id = {};
    };

    struct CreateTaskImageTask
    {
        std::array<TaskImageId, 2> ids = {};
        usize id_count = {};
    };

    struct ImplPresentInfo
    {
        std::vector<BinarySemaphore> binary_semaphores = {};
        std::vector<BinarySemaphore> * additional_binary_semaphores = {};
    };

    struct TaskBatch
    {
        std::vector<usize> pipeline_barrier_indices = {};
        std::vector<usize> wait_split_barrier_indices = {};
        std::vector<TaskId> tasks = {};
        std::vector<usize> signal_split_barrier_indices = {};
    };

    struct TaskBatchSubmitScope
    {
        CommandSubmitInfo submit_info = {};
        TaskSubmitInfo user_submit_info = {};
        // These barriers are inserted after all batches and their sync.
        std::vector<usize> last_minute_barrier_indices = {};
        std::vector<TaskBatch> task_batches = {};
        std::vector<u64> used_swapchain_task_images = {};
        std::optional<ImplPresentInfo> present_info = {};
    };

    auto task_image_access_to_layout_access(TaskImageAccess const & access) -> std::tuple<ImageLayout, Access>;
    auto task_buffer_access_to_access(TaskBufferAccess const & access) -> Access;

    struct TaskListCondition
    {
        bool * condition = {};
    };

    struct ImplTaskList;

    struct TaskListPermutation
    {
        // record time information:
        bool active = {};
        // persistent information:
        TaskImageId swapchain_image = {};
        std::vector<PerPermTaskBuffer> buffer_infos = {};
        std::vector<PerPermTaskImage> image_infos = {};
        std::vector<Task> tasks = {};
        std::vector<TaskSplitBarrier> split_barriers = {};
        std::vector<TaskBarrier> barriers = {};
        std::vector<usize> initial_barriers = {};
        std::vector<TaskBatchSubmitScope> batch_submit_scopes = {};
        usize swapchain_image_first_use_submit_scope_index = std::numeric_limits<usize>::max();
        usize swapchain_image_last_use_submit_scope_index = std::numeric_limits<usize>::max();

        void add_task(ImplTaskList & task_list_impl,
                      TaskInfo const & info,
                      ShaderUseIdToOffsetTable const & shader_id_use_to_offset_table,
                      std::array<std::unordered_map<u32, u32>, 2> const & alias_indirection);
        void submit(TaskSubmitInfo const & info);
        void present(TaskPresentInfo const & info);
    };

    struct ImplPersistentTaskBuffer final : ManagedSharedState
    {
        ImplPersistentTaskBuffer(TaskBufferInfo const & a_info);
        virtual ~ImplPersistentTaskBuffer() override final;

        TaskBufferInfo info = {};
        std::vector<BufferId> actual_buffers = {};
        Access latest_access = {};

        // Used to allocate id - because all persistent resources have unique id we need a single point
        // from which they are generated
        static inline std::atomic_uint32_t exec_unique_next_index = 1;
        u32 unique_index = std::numeric_limits<u32>::max();
    };

    struct ImplPersistentTaskImage final : ManagedSharedState
    {
        ImplPersistentTaskImage(TaskImageInfo const & a_info);
        virtual ~ImplPersistentTaskImage() override final;

        TaskImageInfo info = {};
        // One task buffer can back multiple buffers.
        std::vector<ImageId> actual_images = {};
        // We store runtime information about the previous executions final resource states.
        // This is important, as with conditional execution and temporal resources we need to store this infomation to form correct state transitions.
        std::vector<ImageSliceState> latest_slice_states = {};

        // Used to allocate id - because all persistent resources have unique id we need a single point
        // from which they are generated
        static inline std::atomic_uint32_t exec_unique_next_index = 1;
        u32 unique_index = std::numeric_limits<u32>::max();
    };

    struct PermIndepTaskBufferInfo
    {
        struct Persistent
        {
            ManagedWeakPtr buffer = {};

            auto get() -> ImplPersistentTaskBuffer &
            {
                return *buffer.as<ImplPersistentTaskBuffer>();
            }
            auto get() const -> ImplPersistentTaskBuffer const &
            {
                return *buffer.as<ImplPersistentTaskBuffer>();
            }
        };
        struct Transient
        {
            TransientBufferInfo info = {};
            BufferId buffer = {};
        };
        std::variant<Persistent, Transient> task_buffer_data = {};

        inline auto get_name() const -> std::string_view
        {
            if (is_persistent())
            {
                return get_persistent().info.name;
            }
            else
            {
                return std::get<Transient>(task_buffer_data).info.name;
            }
        }
        inline auto get_persistent() -> ImplPersistentTaskBuffer &
        {
            return std::get<Persistent>(task_buffer_data).get();
        }
        inline auto get_persistent() const -> ImplPersistentTaskBuffer const &
        {
            return std::get<Persistent>(task_buffer_data).get();
        }
        inline auto is_persistent() const -> bool
        {
            return std::holds_alternative<Persistent>(task_buffer_data);
        }
    };

    struct PermIndepTaskImageInfo
    {
        struct Persistent
        {
            ManagedWeakPtr image = {};
            auto get() -> ImplPersistentTaskImage &
            {
                return *image.as<ImplPersistentTaskImage>();
            }
            auto get() const -> ImplPersistentTaskImage const &
            {
                return *image.as<ImplPersistentTaskImage>();
            }
        };
        struct Transient
        {
            TransientImageInfo info = {};
        };
        std::variant<Persistent, Transient> task_image_data = {};

        inline auto get_name() const -> std::string_view
        {
            if (is_persistent())
            {
                return get_persistent().info.name;
            }
            else
            {
                return std::get<Transient>(task_image_data).info.name;
            }
        }
        inline auto get_persistent() -> ImplPersistentTaskImage &
        {
            return std::get<Persistent>(task_image_data).get();
        }
        inline auto get_persistent() const -> ImplPersistentTaskImage const &
        {
            return std::get<Persistent>(task_image_data).get();
        }
        inline auto is_persistent() const -> bool
        {
            return std::holds_alternative<Persistent>(task_image_data);
        }
    };

    struct ImplTaskList final : ManagedSharedState
    {
        ImplTaskList(TaskListInfo a_info);
        virtual ~ImplTaskList() override final;

        static inline std::atomic_uint32_t exec_unique_next_index = 1;
        u32 unique_index = {};

        TaskListInfo info;
        std::vector<PermIndepTaskBufferInfo> global_buffer_infos = {};
        std::vector<PermIndepTaskImageInfo> global_image_infos = {};
        std::vector<TaskListCondition> conditions = {};
        std::vector<TaskListPermutation> permutations = {};
        // TODO: replace with faster hash map.
        /// @brief map the persistent id to a task list local id.
        std::unordered_map<u32, u32> persistent_buffer_index_to_local_index;
        std::unordered_map<u32, u32> persistent_image_index_to_local_index;

        // record time information:
        u32 record_active_conditional_scopes = {};
        u32 record_conditional_states = {};
        std::vector<TaskListPermutation *> record_active_permutations = {};
        std::unordered_map<std::string, TaskBufferId> buffer_name_to_id = {};
        std::unordered_map<std::string, TaskImageId> image_name_to_id = {};
        bool compiled = {};

        // execution time information:
        daxa::TransferMemoryPool staging_memory{TransferMemoryPoolInfo{.device = info.device, .capacity = info.staging_memory_pool_size, .use_bar_memory = true, .name = info.name}};
        std::array<bool, DAXA_TASKLIST_MAX_CONITIONALS> execution_time_current_conditionals = {};

        // post execution information:
        usize last_execution_staging_timeline_value = 0;
        u32 chosen_permutation_last_execution = {};
        std::vector<CommandList> left_over_command_lists = {};
        bool executed_once = {};
        u32 prev_frame_permutation_index = {};
        std::stringstream debug_string_stream = {};

        auto get_actual_buffers(TaskBufferId id) const -> std::span<BufferId const>;
        auto get_actual_images(TaskImageId id, TaskListPermutation const & perm) const -> std::span<ImageId const>;
        auto id_to_local_id(TaskBufferId id) const -> TaskBufferId;
        auto id_to_local_id(TaskImageId id) const -> TaskImageId;
        void update_active_permutations();
        void update_image_view_cache(Task & task, TaskListPermutation const & permutation);

        void create_transient_runtime_buffers();
        void create_transient_runtime_images(TaskListPermutation & permutation);

        void debug_print_memory_barrier(MemoryBarrierInfo & barrier, std::string_view prefix);
        void debug_print_image_memory_barrier(ImageBarrierInfo & barrier, PermIndepTaskImageInfo & glob_image, std::string_view prefix);
        void debug_print_task_barrier(TaskBarrier & barrier, TaskListPermutation const & permutation, usize index, std::string_view prefix);
        void debug_print_task_split_barrier(TaskSplitBarrier & barrier, TaskListPermutation const & permutation, usize index, std::string_view prefix);
        void debug_print_task(TaskListPermutation const & permutation, Task & task, usize task_id, std::string_view prefix);
        void debug_print_permutation_image(TaskListPermutation const & permutation, TaskImageId const image_id);
        void debug_print_permutation_buffer(TaskListPermutation const & permutation, TaskBufferId const buffer_id);
        void debug_print();
        void output_graphviz();
    };

    struct ImplTaskRuntimeInterface
    {
        // interface:
        ImplTaskList & task_list;
        TaskListPermutation & permutation;
        Task * current_task = {};
        bool reuse_last_command_list = true;
        std::vector<CommandList> command_lists = {};
        std::optional<BinarySemaphore> last_submit_semaphore = {};
    };
} // namespace daxa
