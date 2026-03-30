#include <campello_widgets/widgets/platform_menu_delegate.hpp>

namespace systems::leal::campello_widgets
{

    // Static members
    std::unique_ptr<PlatformMenuDelegate> PlatformMenuDelegate::instance_;
    std::mutex PlatformMenuDelegate::instance_mutex_;

    PlatformMenuDelegate* PlatformMenuDelegate::instance()
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            // Return a no-op implementation by default
            // Platform-specific code should call setInstance() to install
            // the appropriate implementation
            static NoOpPlatformMenuDelegate no_op;
            return &no_op;
        }
        return instance_.get();
    }

    void PlatformMenuDelegate::setInstance(std::unique_ptr<PlatformMenuDelegate> delegate)
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        instance_ = std::move(delegate);
    }

    MenuCallbackId PlatformMenuDelegate::registerCallback(std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        MenuCallbackId id = next_callback_id_++;
        
        // Ensure vector is large enough
        if (id >= callbacks_.size()) {
            callbacks_.resize(id + 1);
        }
        callbacks_[id] = std::move(callback);
        return id;
    }

    void PlatformMenuDelegate::clearCallbacks()
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_.clear();
        next_callback_id_ = 1;
    }

    void PlatformMenuDelegate::invokeCallback(MenuCallbackId callback_id)
    {
        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            if (callback_id < callbacks_.size() && callbacks_[callback_id]) {
                callback = callbacks_[callback_id];
            }
        }
        
        // Invoke callback outside the lock
        if (callback) {
            callback();
        }
    }

} // namespace systems::leal::campello_widgets
