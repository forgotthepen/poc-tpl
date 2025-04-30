/*
MIT License

Copyright (c) 2025 forgotthepen (https://github.com/forgotthepen/poc-tpl)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "observable/queue.hpp"
#include <type_traits> // std::enable_if ...
#include <utility> // std::forward
#include <algorithm> // std::find_if, std::min_element ...
#include <memory> // std::unique_ptr ...
#include <cstdlib> // size_t
#include <list>
#include <mutex>
#include <future>
#include <exception>
#include <chrono>


namespace obs {
    class task {
    private:
        template<typename Tfn>
        using t_fn_ret = decltype(std::declval<Tfn>() ());

        class t_worker {
        private:
            class t_waitable_call {
            private:
                struct IWaitableCall {
                    IWaitableCall() = default;
                    IWaitableCall(IWaitableCall &&) = default;
                    IWaitableCall(const IWaitableCall &) = delete;

                    // impl/child class has a dtor, if we don't define virtual dtor here,
                    // `IWaitableCall *ptr = new WaitableCallImpl` won't know that the actual object has dtor
                    // and child will leak its resources when deleted, happens because child type was partially erased
                    virtual ~IWaitableCall() { }

                    IWaitableCall& operator =(IWaitableCall &&) = default;
                    IWaitableCall& operator =(const IWaitableCall &) = delete;

                    virtual void operator ()() noexcept = 0;
                };

                template<typename Tret, typename Tfn>
                class WaitableCallImpl : public IWaitableCall {
                private:
                    std::promise<Tret> promise_;
                    Tfn fn_;

                    template<typename TPromiseType>
                    inline typename std::enable_if< std::is_void<TPromiseType>::value >::type dispatch(int) {
                        fn_();
                        promise_.set_value();
                    }

                    template<typename TPromiseType>
                    inline void dispatch(...) {
                        promise_.set_value( fn_() );
                    }

                public:
                    WaitableCallImpl(std::promise<Tret> &&promise, Tfn &&fn):
                        promise_( std::move(promise) ),
                        fn_( std::move(fn) )
                    { }

                    void operator ()() noexcept override {
                        try {
                            dispatch<Tret>(0);
                        } catch (...) {
                            promise_.set_exception(std::current_exception());
                        }
                    }
                };

                std::unique_ptr<IWaitableCall> waitable_call_;

            public:
                template<typename Tret, typename Tfn>
                t_waitable_call(std::promise<Tret> &&promise, Tfn &&fn):
                    waitable_call_(new WaitableCallImpl<Tret, Tfn>(
                        std::move(promise),
                        std::forward<Tfn>(fn)
                    ))
                { }

                inline void operator ()() const noexcept {
                    (*waitable_call_)();
                }
            };

            obs::queue<t_waitable_call>work_items_{};

            inline void run_work_item(t_waitable_call &work) const noexcept {
                work();
            }
            
        public:
            t_worker() {
                work_items_ += [this](t_waitable_call &work){ run_work_item(work); };
            }

            std::size_t size() {
                return work_items_.size();
            }

            template<typename Tfn, typename Tret>
            std::shared_future<Tret> push_action(Tfn &&action) {
                std::promise<Tret> promise{};
                auto future = std::shared_future<Tret>(promise.get_future());

                work_items_.emplace_back(
                    t_waitable_call(std::move(promise), std::forward<Tfn>(action))
                );

                return future;
            }
        };

        template<typename Rep, typename Period>
        class CancellableDelayImpl {
        private:
            std::chrono::duration<Rep, Period> duration_;
            std::mutex mtx_{};
            std::condition_variable cv_{};
            bool kill_ = false;

        public:
            CancellableDelayImpl(const std::chrono::duration<Rep, Period> &duration):
                duration_(duration)
            { }

            ~CancellableDelayImpl() {
                cancel();
            }

            void delay() {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait_for(lock, duration_, [this]{
                    return kill_;
                });
            }

            void cancel() {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    if (kill_) {
                        return;
                    }
    
                    kill_ = true;
                }

                cv_.notify_one();
            }
        };

        std::list<t_worker> workers_{};
        std::mutex mtx_{};

        unsigned int max_workers = 4;

        template<typename ...Args>
        void sink_template_args(Args&& ...) { }

        template<typename Tfn, typename Tret = t_fn_ret<Tfn>>
        std::shared_future<Tret> run_internal(Tfn &&action) {
            std::lock_guard<std::mutex> lock(mtx_);
    
            {
                if (workers_.empty()) {
                    workers_.emplace_back();
                    auto &worker = workers_.back();
                    return worker.template push_action<Tfn, Tret>(std::forward<Tfn>(action));
                }
            }
    
            {
                auto it_idle = std::find_if(workers_.begin(), workers_.end(), [](t_worker &item){
                    return item.size() <= 0;
                });
                if (it_idle != workers_.end()) {
                    return it_idle->template push_action<Tfn, Tret>(std::forward<Tfn>(action));
                }
            }
    
            {
                if (workers_.size() < static_cast<std::size_t>(max_workers)) {
                    workers_.emplace_back();
                    auto &worker = workers_.back();
                    return worker.template push_action<Tfn, Tret>(std::forward<Tfn>(action));
                }
            }
    
            {
                auto it_min_actions = std::min_element(workers_.begin(), workers_.end(), [](t_worker &item_a, t_worker &item_b){
                    auto size_aa = item_a.size();
                    auto size_bb = item_b.size();
                    return size_aa < size_bb;
                });
                
                return it_min_actions->template push_action<Tfn, Tret>(std::forward<Tfn>(action));
            }
        }

    public:
        template<typename Tret>
        class t_waitable {
        private:
            std::shared_future<Tret> future_;
        
        public:
            t_waitable(std::shared_future<Tret> &&future):
                future_( std::move(future) )
            { }

            inline std::shared_future<Tret> get_future() {
                return future_;
            }

            inline decltype(future_.get()) result() const {
                return future_.get();
            }

            inline bool is_done() const {
                return future_.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready;
            }
        };

        template<typename Rep, typename Period>
        class t_cancellable_delay : public t_waitable<void> {
        private:
            std::unique_ptr<CancellableDelayImpl<Rep, Period>> cancellable_delay_;

        public:
            t_cancellable_delay(std::shared_future<void> &&future, CancellableDelayImpl<Rep, Period> *cancellable_delay):
                t_waitable<void>( std::move(future) ),
                cancellable_delay_(cancellable_delay)
            { }

            inline void cancel() {
                cancellable_delay_->cancel();
            }
            
            inline void wait() {
                result();
            }
        };

        void set_max_workers(unsigned int max) {
            if (max <= 0) {
                throw std::runtime_error("max workers must be > 0");
            }
    
            max_workers = max;
        }

        template<typename Tfn, typename Tret = t_fn_ret<Tfn>>
        inline t_waitable<Tret> run(Tfn &&action) {
            return run_internal( std::forward<Tfn>(action) );
        }

        template<typename ...TWaitables>
        inline void when_all(TWaitables& ...waitables) {
            sink_template_args(( (void)waitables.get_future().wait(), char{} ) ...);
        }

        template<typename Rep, typename Period>
        t_cancellable_delay<Rep, Period> delay(const std::chrono::duration<Rep, Period> &duration) {
            auto delayable = new CancellableDelayImpl<Rep, Period>(duration);
            auto future = run_internal([delayable]{
                delayable->delay();
            });

            return t_cancellable_delay<Rep, Period>(std::move(future), delayable);
        }
    };
}
