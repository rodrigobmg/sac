/*
	This file is part of RecursiveRunner.

	@author Soupe au Caillou - Pierre-Eric Pelloux-Prayer
	@author Soupe au Caillou - Gautier Pelloux-Prayer

	RecursiveRunner is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 3.

	RecursiveRunner is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with RecursiveRunner.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <string>
#include <map>
#include <list>
#include <queue>

class IStorageProxy {
    public:
        virtual std::string getValue(const std::string& columnName) = 0;
        virtual void setValue(const std::string& columnName, const std::string& value) = 0;
        virtual void pushAnElement() = 0;
        virtual bool popAnElement() = 0;
        virtual const std::string & getTableName() = 0;
        virtual const std::map<std::string, std::string> & getColumnsNameAndType() = 0;
};

template <class T>
class StorageProxy : public IStorageProxy {
    public:
        virtual std::string getValue(const std::string& columnName) = 0;
        virtual void setValue(const std::string& columnName, const std::string& value) = 0;

        void pushAnElement() {
            _queue.push(T());
        }

        bool popAnElement() {
            _queue.pop();

            return (! _queue.empty());
        }

        const std::string & getTableName() {
            return _tableName;
        }

        const std::map<std::string, std::string> & getColumnsNameAndType() {
            return _columnsNameAndType;
        }

    public:
        std::queue<T> _queue;

    protected:
        std::string _tableName;
        std::map<std::string, std::string> _columnsNameAndType;
};

