#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts
 *   
 *   This file is part of TADS 3.  
 */

#ifndef _ARRAY_H_
#define _ARRAY_H_

/*
 *   'Array' intrinsic class
 */
intrinsic class Array 'array/030006'
{
    /* 
     *   Create a list with the same elements as the array.  If 'start' is
     *   specified, it's the index of the first element we store; we'll
     *   store elements starting at index 'start'.  If 'cnt' is specified,
     *   it gives the maximum number of elements for the new list; by
     *   default, we'll store all of the elements from 'start' to the last
     *   element.  
     */
    toList(start?, cnt?);

    /* get the number of elements in the array */
    length();

    /* 
     *   Copy from another array or list.  Elements are copied from the
     *   source array or list starting at the element given by
     *   'src_start', and are copied into 'self' starting at the index
     *   given by 'dst_start'.  At most 'cnt' values are copied, but we
     *   stop when we reach the last element of either the source or
     *   destination values.  
     */
    copyFrom(src, src_start, dst_start, cnt);

    /* 
     *   fill with a given value, starting at the given element (the first
     *   element if not specified), and running for the given number of
     *   elements (the remaining elements of the array, if not not
     *   specified) 
     */
    fillValue(val, start?, cnt?);

    /*
     *   Select a subset of the array.  Returns a new array consisting
     *   only of the elements of this array for which the callback
     *   function returns true.  
     */
    subset(func);

    /*
     *   Apply a callback function to each element of the array.  For each
     *   element of the array, invokes the callback, and replaces the
     *   element with the return value of the callback.  Modifies the
     *   array in-place, and returns 'self'. 
     */
    applyAll(func);

    /* 
     *   Find the first element for which the given condition is true.
     *   Apply the callback function (which encodes the condition to
     *   evaluate) to each element in turn, starting with the first.  For
     *   each element, if the callback returns nil, proceed to the next
     *   element; otherwise, stop and return the index of the element.  If
     *   the callback never returns true for any element, we'll return
     *   nil.  
     */
    indexWhich(cond);

    /* 
     *   Invoke the callback on each element, in order from first to last.
     *   No return value.  
     */
    forEach(func);

    /*
     *   Apply the callback function to each element of this array, and
     *   return a new array consisting of the results.  Effectively maps
     *   the array to a new array using the given function, leaving the
     *   original array unchanged.  
     */
    mapAll(func);

    /* get the index of the first match for the given value */
    indexOf(val);

    /* 
     *   Find the first element for which the given condition is true, and
     *   return the value of the element.  
     */
    valWhich(cond);

    /* find the last element with the given value, and return its index */
    lastIndexOf(val);

    /* 
     *   Find the last element for which the condition is true, and return
     *   the index of the element.  Applies the callback to each element
     *   in turn, starting with the last element and working backwards.
     *   For each element, if the callback returns nil, proceeds to the
     *   previous element; otherwise, stops and returns the index of the
     *   element.  If the callback never returns true for any element,
     *   we'll return nil.  
     */
    lastIndexWhich(cond);

    /* 
     *   Find the last element for which the condition is true, and return
     *   the value of the element 
     */
    lastValWhich(cond);

    /* count the number of elements with the given value */
    countOf(val);

    /* count the number of elements for which the callback returns true */
    countWhich(cond);

    /* create a new array consisting of the unique elements of this array */
    getUnique();

    /*
     *   append the elements of the list or array 'val' to the elements of
     *   this array, then remove repeated elements in the result; returns
     *   a new array with the unique elements of the combination 
     */
    appendUnique(val);

    /* 
     *   Sort the array in place; returns 'self'.  If the 'descending'
     *   flag is provided and is not nil, we'll sort the array in
     *   descending order rather than ascending order.
     *   
     *   If the 'comparisonFunction' argument is provided, it must be a
     *   callback function; the callback takes two arguments, and returns
     *   an integer less than zero if the first argument value is less
     *   than the second, zero if they're equal, and an integer greater
     *   than zero if the first is greater than the second.  If no
     *   'comparisonFunction' argument is provided, or it's provided and
     *   its value is nil, we'll simply compare the array elements as
     *   ordinary values.  The comparison function can be provided for
     *   caller-defined orderings, such as ordering a set of objects.  
     */
    sort(descending?, comparisonFunction?);

    /* 
     *   Append an element to the vector.  This works just like insertAt()
     *   with a starting index one higher than the length of the vector.
     *   This has almost the same effect as the '+' operator, but treats a
     *   list value like any other value by simply inserting the list as a
     *   single new element (rather than appending each item in the list
     *   individually, as the '+' operator would).  
     */
    append(val);
}

#endif /* _ARRAY_H_ */

