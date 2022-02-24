#ifndef DELETIONQUEUE_HPP
#define DELETIONQUEUE_HPP

#include <functional>
#include <deque>

struct DeletionQueue {
	std::deque<std::function<void()>> queue;

	void push(std::function<void()>&& function) {
		queue.push_back(function);
	}
	void clear() {
		for(auto i = queue.rbegin(); i != queue.rend(); i++)
			(*i)();
		queue.clear();
	}
};

#endif //DELETIONQUEUE_HPP
